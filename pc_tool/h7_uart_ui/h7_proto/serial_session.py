from __future__ import annotations

import time
from collections.abc import Callable

import serial

from .codec import ProtocolStreamParser, build_frame
from .constants import TYPE_EVENT, TYPE_REQ
from .event import decode_mcu_info_event, event_summary
from .frame import ProtoFrame, frame_summary


EventCallback = Callable[[ProtoFrame], None]


class H7SerialSession:
    def __init__(
        self,
        port: str,
        baud: int = 115200,
        serial_timeout: float = 0.02,
        write_timeout: float = 0.5,
    ) -> None:
        self.port = port
        self.baud = baud
        self.serial_timeout = serial_timeout
        self.write_timeout = write_timeout

        self.ser: serial.Serial | None = None
        self.parser = ProtocolStreamParser()

        self.event_count = 0
        self.non_event_count = 0

    def open(self) -> None:
        self.ser = serial.Serial(
            port=self.port,
            baudrate=self.baud,
            timeout=self.serial_timeout,
            write_timeout=self.write_timeout,
        )
        time.sleep(0.2)
        self.flush()

    def close(self) -> None:
        if self.ser is not None and self.ser.is_open:
            self.ser.close()

    def __enter__(self) -> "H7SerialSession":
        self.open()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def flush(self) -> None:
        if self.ser is None:
            return

        time.sleep(0.05)

        try:
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
        except Exception:
            pass

        self.parser.reset()

    def write_raw(self, data: bytes) -> None:
        if self.ser is None:
            raise RuntimeError("serial session is not open")

        self.ser.write(data)

    def write_frame(
        self,
        frame_type: int,
        seq: int,
        cmd: int,
        payload: bytes = b"",
        flags: int = 0,
    ) -> bytes:
        frame = build_frame(frame_type, flags, seq, cmd, payload)
        self.write_raw(frame)
        return frame

    def write_request(self, seq: int, cmd: int, payload: bytes = b"") -> bytes:
        return self.write_frame(TYPE_REQ, seq, cmd, payload)

    def read_available_frames(self) -> list[ProtoFrame]:
        if self.ser is None:
            raise RuntimeError("serial session is not open")

        n = self.ser.in_waiting

        if n <= 0:
            return []

        data = self.ser.read(n)
        frames = self.parser.feed(data)

        for frame in frames:
            if frame.frame_type == TYPE_EVENT:
                self.event_count += 1
            else:
                self.non_event_count += 1

        return frames

    def read_frames_for(self, duration_s: float) -> list[ProtoFrame]:
        deadline = time.time() + duration_s
        frames: list[ProtoFrame] = []

        while time.time() < deadline:
            frames.extend(self.read_available_frames())
            time.sleep(0.003)

        return frames

    def read_until(
        self,
        predicate: Callable[[ProtoFrame], bool],
        timeout_s: float,
        event_callback: EventCallback | None = None,
        show_ignored_frames: bool = False,
    ) -> ProtoFrame | None:
        deadline = time.time() + timeout_s

        while time.time() < deadline:
            frames = self.read_available_frames()

            for frame in frames:
                if frame.is_event():
                    if event_callback is not None:
                        event_callback(frame)
                    else:
                        event = decode_mcu_info_event(frame)
                        print(f"[EVENT] {event_summary(event)}")
                    continue

                if predicate(frame):
                    return frame

                if show_ignored_frames:
                    print(f"[PC] ignored frame: {frame_summary(frame)}")

            time.sleep(0.003)

        return None