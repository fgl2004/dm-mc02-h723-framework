from __future__ import annotations

import argparse
import time
from dataclasses import dataclass

import serial


SOF1 = 0xA5
SOF2 = 0x5A
VER = 0x01

TYPE_REQ = 0x01
TYPE_RESP = 0x02
TYPE_NACK = 0x04

CMD_PING = 0x01
CMD_GET_VERSION = 0x02
CMD_GET_STATUS = 0x03

MAX_PAYLOAD = 128


@dataclass
class ProtoFrame:
    frame_type: int
    flags: int
    seq: int
    cmd: int
    payload: bytes


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF

    for b in data:
        crc ^= b << 8

        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF

    return crc


def build_frame(frame_type: int, flags: int, seq: int, cmd: int, payload: bytes = b"") -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload too large")

    length = len(payload)

    header = bytes(
        [
            VER,
            frame_type & 0xFF,
            flags & 0xFF,
            seq & 0xFF,
            cmd & 0xFF,
            length & 0xFF,
            (length >> 8) & 0xFF,
        ]
    )

    crc_input = header + payload
    crc = crc16_ccitt_false(crc_input)

    return bytes([SOF1, SOF2]) + crc_input + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def try_parse_one_frame(buf: bytearray) -> ProtoFrame | None:
    while len(buf) >= 2:
        if buf[0] == SOF1 and buf[1] == SOF2:
            break
        del buf[0]

    if len(buf) < 11:
        return None

    ver = buf[2]
    frame_type = buf[3]
    flags = buf[4]
    seq = buf[5]
    cmd = buf[6]
    length = buf[7] | (buf[8] << 8)

    if ver != VER:
        del buf[0]
        return None

    if length > MAX_PAYLOAD:
        del buf[0]
        return None

    total_len = 2 + 7 + length + 2

    if len(buf) < total_len:
        return None

    frame_bytes = bytes(buf[:total_len])
    del buf[:total_len]

    crc_recv = frame_bytes[-2] | (frame_bytes[-1] << 8)
    crc_calc = crc16_ccitt_false(frame_bytes[2:-2])

    if crc_recv != crc_calc:
        print(f"[PC] CRC error: recv=0x{crc_recv:04X}, calc=0x{crc_calc:04X}")
        return None

    payload = frame_bytes[9:-2]

    return ProtoFrame(
        frame_type=frame_type,
        flags=flags,
        seq=seq,
        cmd=cmd,
        payload=payload,
    )


def read_response(ser: serial.Serial, timeout_s: float) -> ProtoFrame | None:
    deadline = time.time() + timeout_s
    rx_buf = bytearray()

    while time.time() < deadline:
        n = ser.in_waiting

        if n > 0:
            rx_buf.extend(ser.read(n))

            frame = try_parse_one_frame(rx_buf)
            if frame is not None:
                return frame

        time.sleep(0.005)

    return None


def send_command(port: str, baud: int, cmd: int, seq: int, timeout: float) -> None:
    tx = build_frame(TYPE_REQ, 0, seq, cmd, b"")

    print(f"[PC] Open {port} @ {baud}")

    with serial.Serial(port=port, baudrate=baud, timeout=0.02, write_timeout=0.2) as ser:
        time.sleep(0.2)
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        print("[PC] TX:", tx.hex(" ").upper())
        ser.write(tx)

        frame = read_response(ser, timeout)

        if frame is None:
            print("[PC] RX timeout")
            return

        print(
            f"[PC] RX frame: type=0x{frame.frame_type:02X}, "
            f"flags=0x{frame.flags:02X}, seq={frame.seq}, "
            f"cmd=0x{frame.cmd:02X}, len={len(frame.payload)}"
        )

        if frame.payload:
            print("[PC] RX payload HEX:", frame.payload.hex(" ").upper())

            try:
                print("[PC] RX payload ASCII:", frame.payload.decode("utf-8", errors="replace"))
            except Exception:
                pass

        if frame.frame_type == TYPE_RESP:
            print("[PC] RESULT: RESP OK")
        elif frame.frame_type == TYPE_NACK:
            print("[PC] RESULT: NACK")
        else:
            print("[PC] RESULT: unexpected frame type")


def main() -> None:
    parser = argparse.ArgumentParser(description="DM-MC02 H723 UART protocol ping test")
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM19")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--cmd", choices=["ping", "version", "status"], default="ping")
    parser.add_argument("--seq", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=1.0)

    args = parser.parse_args()

    cmd_map = {
        "ping": CMD_PING,
        "version": CMD_GET_VERSION,
        "status": CMD_GET_STATUS,
    }

    send_command(
        port=args.port,
        baud=args.baud,
        cmd=cmd_map[args.cmd],
        seq=args.seq & 0xFF,
        timeout=args.timeout,
    )


if __name__ == "__main__":
    main()