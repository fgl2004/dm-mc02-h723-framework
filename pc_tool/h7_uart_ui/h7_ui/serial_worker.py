from __future__ import annotations

import queue
import threading
import time
from typing import Optional

import serial
import serial.tools.list_ports

from PySide6.QtCore import QObject, Signal

from h7_proto.codec import ProtocolStreamParser, build_frame
from h7_proto.constants import (
    TYPE_ACK,
    TYPE_DATA,
    TYPE_EVENT,
    TYPE_NACK,
    TYPE_REQ,
    TYPE_RESP,
    
    TYPE_WINDOW_ACK,
)
from h7_proto.frame import ProtoFrame

from .telemetry import parse_uartstat_line


def list_serial_ports() -> list[str]:
    ports = serial.tools.list_ports.comports()
    return [p.device for p in ports]


class SerialWorker(QObject):
    connected = Signal(str)
    disconnected = Signal()
    error = Signal(str)

    raw_line = Signal(str)
    uart_stat = Signal(object)

    protocol_frame = Signal(object)
    protocol_event = Signal(object)
    protocol_response = Signal(object)
    protocol_nack = Signal(object)
    protocol_data = Signal(object)
    protocol_ack = Signal(object)
    protocol_window_ack = Signal(object)

    tx_bytes_written = Signal(int)
    tx_queue_size_changed = Signal(int)

    def __init__(self) -> None:
        super().__init__()

        self._ser: Optional[serial.Serial] = None
        self._thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()

        self._lock = threading.Lock()
        self._rx_line_buffer = bytearray()
        self._proto_parser = ProtocolStreamParser()

        self._tx_queue: queue.Queue[bytes] = queue.Queue()
        self._tx_queue_size = 0

        self._tx_chunk_max = 32
        self._seq = 0

    def connect_port(self, port: str, baud: int) -> None:
        print(f"[DEBUG] SerialWorker.connect_port({port}, {baud})")

        if self.is_open():
            self.disconnect_port()

        try:
            ser = serial.Serial(
                port=port,
                baudrate=baud,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.02,
                write_timeout=0.05,
            )

            with self._lock:
                self._ser = ser
                self._rx_line_buffer.clear()
                self._proto_parser.reset()
                self._seq = 0
                self._clear_tx_queue_locked()

            self._stop_event.clear()

            self._thread = threading.Thread(
                target=self._io_loop,
                name="H7SerialWorker",
                daemon=True,
            )
            self._thread.start()

            self.connected.emit(f"Connected to {port} @ {baud}")

        except Exception as exc:
            self.error.emit(f"Open serial failed: {exc}")

    def disconnect_port(self) -> None:
        print("[DEBUG] SerialWorker.disconnect_port()")

        self._stop_event.set()

        with self._lock:
            self._clear_tx_queue_locked()

            if self._ser is not None:
                try:
                    if self._ser.is_open:
                        self._ser.close()
                except Exception:
                    pass

                self._ser = None

            self._rx_line_buffer.clear()
            self._proto_parser.reset()

        if self._thread is not None and self._thread.is_alive():
            self._thread.join(timeout=0.5)

        self._thread = None

        self.tx_queue_size_changed.emit(0)
        self.disconnected.emit()

    def is_open(self) -> bool:
        with self._lock:
            return self._ser is not None and self._ser.is_open

    def enqueue_tx(self, data: bytes) -> None:
        print(f"[DEBUG] SerialWorker.enqueue_tx({len(data)} bytes)")

        if not data:
            return

        with self._lock:
            if self._ser is None or not self._ser.is_open:
                raise RuntimeError("serial port is not open")

            data_bytes = bytes(data)

            for offset in range(0, len(data_bytes), self._tx_chunk_max):
                chunk = data_bytes[offset: offset + self._tx_chunk_max]
                self._tx_queue.put(chunk)
                self._tx_queue_size += len(chunk)

            queue_size = self._tx_queue_size

        self.tx_queue_size_changed.emit(queue_size)

    def send_frame(
        self,
        frame_type: int,
        seq: int,
        cmd: int,
        payload: bytes = b"",
        flags: int = 0,
    ) -> bytes:
        frame = build_frame(
            frame_type=frame_type,
            flags=flags,
            seq=seq,
            cmd=cmd,
            payload=payload,
        )

        self.enqueue_tx(frame)
        return frame

    def send_request(self, cmd: int, payload: bytes = b"") -> tuple[int, bytes]:
        seq = self._next_seq()
        frame = self.send_frame(TYPE_REQ, seq, cmd, payload)
        return seq, frame

    def send_ack(self, seq: int, cmd: int, payload: bytes = b"", flags: int = 0) -> bytes:
        return self.send_frame(TYPE_ACK, seq, cmd, payload, flags)

    def send_data(self, seq: int, cmd: int, payload: bytes = b"", flags: int = 0) -> bytes:
        return self.send_frame(TYPE_DATA, seq, cmd, payload, flags)

    def send_window_ack(self, seq: int, cmd: int, payload: bytes = b"", flags: int = 0) -> bytes:
        return self.send_frame(TYPE_WINDOW_ACK, seq, cmd, payload, flags)

    def clear_tx_queue(self) -> None:
        print("[DEBUG] SerialWorker.clear_tx_queue()")

        with self._lock:
            self._clear_tx_queue_locked()

        self.tx_queue_size_changed.emit(0)

    def get_tx_queue_size(self) -> int:
        with self._lock:
            return self._tx_queue_size

    def _next_seq(self) -> int:
        with self._lock:
            self._seq = (self._seq + 1) & 0xFF

            if self._seq == 0:
                self._seq = 1

            return self._seq

    def _clear_tx_queue_locked(self) -> None:
        while not self._tx_queue.empty():
            try:
                self._tx_queue.get_nowait()
            except queue.Empty:
                break

        self._tx_queue_size = 0

    def _get_serial(self) -> Optional[serial.Serial]:
        with self._lock:
            return self._ser

    def _io_loop(self) -> None:
        print("[DEBUG] SerialWorker IO thread started")

        while not self._stop_event.is_set():
            ser = self._get_serial()

            if ser is None or not ser.is_open:
                break

            self._poll_rx(ser)
            self._poll_tx(ser)

            time.sleep(0.001)

        print("[DEBUG] SerialWorker IO thread stopped")

    def _poll_rx(self, ser: serial.Serial) -> None:
        try:
            n = ser.in_waiting

            if n <= 0:
                return

            data = ser.read(n)

            if not data:
                return

            self._process_protocol_bytes(data)
            self._process_text_lines(data)

        except Exception as exc:
            if not self._stop_event.is_set():
                self.error.emit(f"Serial RX error: {exc}")

    def _process_protocol_bytes(self, data: bytes) -> None:
        frames = self._proto_parser.feed(data)

        for frame in frames:
            self.protocol_frame.emit(frame)

            if frame.frame_type == TYPE_EVENT:
                self.protocol_event.emit(frame)
            elif frame.frame_type == TYPE_RESP:
                self.protocol_response.emit(frame)
            elif frame.frame_type == TYPE_NACK:
                self.protocol_nack.emit(frame)
            elif frame.frame_type == TYPE_DATA:
                self.protocol_data.emit(frame)
            elif frame.frame_type == TYPE_ACK:
                self.protocol_ack.emit(frame)
            elif frame.frame_type == TYPE_WINDOW_ACK:
                self.protocol_window_ack.emit(frame)

    def _process_text_lines(self, data: bytes) -> None:
        """
        Text log parser.

        The same UART carries text logs and binary protocol frames.
        Binary frames may contain arbitrary bytes, so this parser only emits
        lines that look like known text logs.
        """
        lines: list[str] = []

        with self._lock:
            self._rx_line_buffer.extend(data)

            if len(self._rx_line_buffer) > 4096:
                self._rx_line_buffer = self._rx_line_buffer[-1024:]

            while b"\n" in self._rx_line_buffer:
                line_bytes, _, remain = self._rx_line_buffer.partition(b"\n")
                self._rx_line_buffer = bytearray(remain)

                line = line_bytes.decode("utf-8", errors="replace").strip("\r\n ")

                marker_positions = [
                    pos for pos in (line.find("@UARTSTAT"), line.find("["))
                    if pos >= 0
                ]

                if marker_positions:
                    line = line[min(marker_positions):].strip()

                if not line:
                    continue

                if line.startswith("@UARTSTAT") or line.startswith("["):
                    lines.append(line)

        for line in lines:
            self.raw_line.emit(line)

            stat = parse_uartstat_line(line, host_time=time.time())
            if stat is not None:
                self.uart_stat.emit(stat)

    def _poll_tx(self, ser: serial.Serial) -> None:
        try:
            try:
                chunk = self._tx_queue.get_nowait()
            except queue.Empty:
                return

            if not chunk:
                return

            try:
                written = ser.write(chunk)
            except serial.SerialTimeoutException:
                written = 0

            if written < 0:
                written = 0

            if written > 0:
                with self._lock:
                    self._tx_queue_size = max(0, self._tx_queue_size - written)
                    queue_size = self._tx_queue_size

                self.tx_bytes_written.emit(written)
                self.tx_queue_size_changed.emit(queue_size)

            if written < len(chunk):
                remain = chunk[written:]

                if remain:
                    with self._lock:
                        self._tx_queue.put(remain)
                        self._tx_queue_size += len(remain)
                        queue_size = self._tx_queue_size

                    self.tx_queue_size_changed.emit(queue_size)

        except Exception as exc:
            if not self._stop_event.is_set():
                self.error.emit(f"Serial TX error: {exc}")
