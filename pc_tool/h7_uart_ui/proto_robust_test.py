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
TYPE_ACK = 0x03
TYPE_NACK = 0x04
TYPE_EVENT = 0x05
TYPE_DATA = 0x06
TYPE_WINDOW_ACK = 0x07

CMD_PING = 0x01
CMD_GET_VERSION = 0x02
CMD_GET_STATUS = 0x03
CMD_UNKNOWN_TEST = 0x7E

PROTO_ERROR_UNKNOWN_CMD = 0x01

MAX_PAYLOAD = 128


@dataclass
class ProtoFrame:
    frame_type: int
    flags: int
    seq: int
    cmd: int
    payload: bytes


@dataclass
class TestResult:
    name: str
    passed: bool
    detail: str


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


def build_frame(
    frame_type: int,
    flags: int,
    seq: int,
    cmd: int,
    payload: bytes = b"",
) -> bytes:
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


def build_frame_with_raw_len(
    frame_type: int,
    flags: int,
    seq: int,
    cmd: int,
    raw_len: int,
    payload: bytes = b"",
) -> bytes:
    """
    Build a frame with a manually specified LEN field.

    This is used to test oversized length handling.
    The frame may be intentionally incomplete from the parser's point of view.
    """
    header = bytes(
        [
            VER,
            frame_type & 0xFF,
            flags & 0xFF,
            seq & 0xFF,
            cmd & 0xFF,
            raw_len & 0xFF,
            (raw_len >> 8) & 0xFF,
        ]
    )

    crc_input = header + payload
    crc = crc16_ccitt_false(crc_input)

    return bytes([SOF1, SOF2]) + crc_input + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def corrupt_last_crc_byte(frame: bytes) -> bytes:
    if len(frame) < 2:
        return frame

    data = bytearray(frame)
    data[-1] ^= 0x55
    return bytes(data)


def try_parse_one_frame(buf: bytearray) -> ProtoFrame | None:
    """
    Parse one valid protocol frame from mixed UART stream.

    MCU UART may also print ASCII logs, so this parser skips all bytes
    before SOF1/SOF2.
    """
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
        print(f"[PC] RX CRC error: recv=0x{crc_recv:04X}, calc=0x{crc_calc:04X}")
        return None

    payload = frame_bytes[9:-2]

    return ProtoFrame(
        frame_type=frame_type,
        flags=flags,
        seq=seq,
        cmd=cmd,
        payload=payload,
    )


def read_frames(
    ser: serial.Serial,
    count: int,
    timeout_s: float,
) -> list[ProtoFrame]:
    deadline = time.time() + timeout_s
    rx_buf = bytearray()
    frames: list[ProtoFrame] = []

    while time.time() < deadline and len(frames) < count:
        n = ser.in_waiting

        if n > 0:
            data = ser.read(n)
            rx_buf.extend(data)

            while True:
                frame = try_parse_one_frame(rx_buf)

                if frame is None:
                    break

                frames.append(frame)

                if len(frames) >= count:
                    break

        time.sleep(0.003)

    return frames


def flush_serial(ser: serial.Serial) -> None:
    time.sleep(0.05)

    try:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
    except Exception:
        pass


def frame_summary(frame: ProtoFrame | None) -> str:
    if frame is None:
        return "None"

    payload_hex = frame.payload.hex(" ").upper()

    try:
        payload_ascii = frame.payload.decode("utf-8", errors="replace")
    except Exception:
        payload_ascii = ""

    return (
        f"type=0x{frame.frame_type:02X}, "
        f"flags=0x{frame.flags:02X}, "
        f"seq={frame.seq}, "
        f"cmd=0x{frame.cmd:02X}, "
        f"len={len(frame.payload)}, "
        f"payload_hex=[{payload_hex}], "
        f"payload_ascii=[{payload_ascii}]"
    )


def expect_resp(
    frame: ProtoFrame | None,
    seq: int,
    cmd: int,
    payload: bytes | None = None,
) -> tuple[bool, str]:
    if frame is None:
        return False, "no response"

    if frame.frame_type != TYPE_RESP:
        return False, f"unexpected type: {frame_summary(frame)}"

    if frame.seq != (seq & 0xFF):
        return False, f"unexpected seq: {frame_summary(frame)}"

    if frame.cmd != (cmd & 0xFF):
        return False, f"unexpected cmd: {frame_summary(frame)}"

    if payload is not None and frame.payload != payload:
        return False, f"unexpected payload: {frame_summary(frame)}"

    return True, frame_summary(frame)


def expect_nack(
    frame: ProtoFrame | None,
    seq: int,
    cmd: int,
    error_code: int | None = None,
) -> tuple[bool, str]:
    if frame is None:
        return False, "no response"

    if frame.frame_type != TYPE_NACK:
        return False, f"unexpected type: {frame_summary(frame)}"

    if frame.seq != (seq & 0xFF):
        return False, f"unexpected seq: {frame_summary(frame)}"

    if frame.cmd != (cmd & 0xFF):
        return False, f"unexpected cmd: {frame_summary(frame)}"

    if error_code is not None:
        if len(frame.payload) < 1:
            return False, f"nack payload too short: {frame_summary(frame)}"

        if frame.payload[0] != (error_code & 0xFF):
            return False, f"unexpected nack error code: {frame_summary(frame)}"

    return True, frame_summary(frame)


def test_ping(ser: serial.Serial, timeout: float) -> TestResult:
    name = "valid_ping"
    seq = 1
    tx = build_frame(TYPE_REQ, 0, seq, CMD_PING)

    flush_serial(ser)
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    ser.write(tx)

    frames = read_frames(ser, 1, timeout)
    frame = frames[0] if frames else None

    ok, detail = expect_resp(frame, seq, CMD_PING, b"PONG")
    return TestResult(name, ok, detail)


def test_get_version(ser: serial.Serial, timeout: float) -> TestResult:
    name = "get_version"
    seq = 2
    tx = build_frame(TYPE_REQ, 0, seq, CMD_GET_VERSION)

    flush_serial(ser)
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    ser.write(tx)

    frames = read_frames(ser, 1, timeout)
    frame = frames[0] if frames else None

    ok, detail = expect_resp(frame, seq, CMD_GET_VERSION)

    if ok and not frame.payload.startswith(b"DM-MC02-H723"):
        return TestResult(name, False, f"unexpected version payload: {frame_summary(frame)}")

    return TestResult(name, ok, detail)


def test_get_status(ser: serial.Serial, timeout: float) -> TestResult:
    name = "get_status"
    seq = 3
    tx = build_frame(TYPE_REQ, 0, seq, CMD_GET_STATUS)

    flush_serial(ser)
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    ser.write(tx)

    frames = read_frames(ser, 1, timeout)
    frame = frames[0] if frames else None

    ok, detail = expect_resp(frame, seq, CMD_GET_STATUS, b"OK")
    return TestResult(name, ok, detail)


def test_unknown_cmd_nack(ser: serial.Serial, timeout: float) -> TestResult:
    name = "unknown_cmd_nack"
    seq = 4
    tx = build_frame(TYPE_REQ, 0, seq, CMD_UNKNOWN_TEST)

    flush_serial(ser)
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    ser.write(tx)

    frames = read_frames(ser, 1, timeout)
    frame = frames[0] if frames else None

    ok, detail = expect_nack(frame, seq, CMD_UNKNOWN_TEST, PROTO_ERROR_UNKNOWN_CMD)
    return TestResult(name, ok, detail)


def test_crc_error_no_response(ser: serial.Serial, timeout: float) -> TestResult:
    name = "crc_error_no_response"
    seq = 5
    good = build_frame(TYPE_REQ, 0, seq, CMD_PING)
    bad = corrupt_last_crc_byte(good)

    flush_serial(ser)
    print(f"[TEST] {name} TX:", bad.hex(" ").upper())
    ser.write(bad)

    frames = read_frames(ser, 1, timeout)

    if frames:
        return TestResult(name, False, f"unexpected response: {frame_summary(frames[0])}")

    return TestResult(name, True, "no response as expected")


def test_garbage_before_valid_frame(ser: serial.Serial, timeout: float) -> TestResult:
    name = "garbage_before_valid_frame"
    seq = 6
    garbage = bytes([0x00, 0x11, 0x22, 0x33, 0xA5, 0xA5, 0x00, 0x55])
    good = build_frame(TYPE_REQ, 0, seq, CMD_PING)
    tx = garbage + good

    flush_serial(ser)
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    ser.write(tx)

    frames = read_frames(ser, 1, timeout)
    frame = frames[0] if frames else None

    ok, detail = expect_resp(frame, seq, CMD_PING, b"PONG")
    return TestResult(name, ok, detail)


def test_half_packet(ser: serial.Serial, timeout: float, gap_s: float) -> TestResult:
    name = "half_packet"
    seq = 7
    tx = build_frame(TYPE_REQ, 0, seq, CMD_PING)

    split_pos = 5

    flush_serial(ser)
    print(f"[TEST] {name} TX part1:", tx[:split_pos].hex(" ").upper())
    ser.write(tx[:split_pos])

    time.sleep(gap_s)

    print(f"[TEST] {name} TX part2:", tx[split_pos:].hex(" ").upper())
    ser.write(tx[split_pos:])

    frames = read_frames(ser, 1, timeout)
    frame = frames[0] if frames else None

    ok, detail = expect_resp(frame, seq, CMD_PING, b"PONG")
    return TestResult(name, ok, detail)


def test_sticky_packets(ser: serial.Serial, timeout: float) -> TestResult:
    name = "sticky_packets"

    tx1 = build_frame(TYPE_REQ, 0, 8, CMD_PING)
    tx2 = build_frame(TYPE_REQ, 0, 9, CMD_GET_STATUS)
    tx3 = build_frame(TYPE_REQ, 0, 10, CMD_GET_VERSION)
    tx = tx1 + tx2 + tx3

    flush_serial(ser)
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    ser.write(tx)

    frames = read_frames(ser, 3, timeout)

    if len(frames) != 3:
        return TestResult(name, False, f"expected 3 frames, got {len(frames)}")

    checks = [
        expect_resp(frames[0], 8, CMD_PING, b"PONG"),
        expect_resp(frames[1], 9, CMD_GET_STATUS, b"OK"),
        expect_resp(frames[2], 10, CMD_GET_VERSION),
    ]

    for ok, detail in checks:
        if not ok:
            return TestResult(name, False, detail)

    return TestResult(name, True, "3 sticky responses parsed")


def test_invalid_type_no_response(ser: serial.Serial, timeout: float) -> TestResult:
    name = "invalid_type_no_response"
    seq = 11
    invalid_type = 0x55
    tx = build_frame(invalid_type, 0, seq, CMD_PING)

    flush_serial(ser)
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    ser.write(tx)

    frames = read_frames(ser, 1, timeout)

    if frames:
        return TestResult(name, False, f"unexpected response: {frame_summary(frames[0])}")

    return TestResult(name, True, "no response as expected")


def test_oversized_len_no_response(ser: serial.Serial, timeout: float) -> TestResult:
    name = "oversized_len_no_response"
    seq = 12
    raw_len = MAX_PAYLOAD + 1
    tx = build_frame_with_raw_len(TYPE_REQ, 0, seq, CMD_PING, raw_len, b"")

    flush_serial(ser)
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    ser.write(tx)

    frames = read_frames(ser, 1, timeout)

    if frames:
        return TestResult(name, False, f"unexpected response: {frame_summary(frames[0])}")

    return TestResult(name, True, "no response as expected")


def run_all_tests(port: str, baud: int, timeout: float, gap: float) -> int:
    results: list[TestResult] = []

    print(f"[PC] Open {port} @ {baud}")

    with serial.Serial(port=port, baudrate=baud, timeout=0.02, write_timeout=0.5) as ser:
        time.sleep(0.2)
        flush_serial(ser)

        tests = [
            lambda: test_ping(ser, timeout),
            lambda: test_get_version(ser, timeout),
            lambda: test_get_status(ser, timeout),
            lambda: test_unknown_cmd_nack(ser, timeout),
            lambda: test_crc_error_no_response(ser, timeout),
            lambda: test_garbage_before_valid_frame(ser, timeout),
            lambda: test_half_packet(ser, timeout, gap),
            lambda: test_sticky_packets(ser, timeout),
            lambda: test_invalid_type_no_response(ser, timeout),
            lambda: test_oversized_len_no_response(ser, timeout),
        ]

        for test_fn in tests:
            result = test_fn()
            results.append(result)

            status = "PASS" if result.passed else "FAIL"
            print(f"[RESULT] {status}: {result.name} - {result.detail}")
            print("-" * 80)

            time.sleep(0.1)

    pass_count = sum(1 for r in results if r.passed)
    fail_count = len(results) - pass_count

    print("=" * 80)
    print(f"[SUMMARY] total={len(results)}, pass={pass_count}, fail={fail_count}")

    for r in results:
        status = "PASS" if r.passed else "FAIL"
        print(f"[SUMMARY] {status}: {r.name} - {r.detail}")

    print("=" * 80)

    return 0 if fail_count == 0 else 1


def main() -> None:
    parser = argparse.ArgumentParser(description="DM-MC02 H723 UART protocol robustness test")
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM19")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument("--gap", type=float, default=0.1, help="Half-packet gap in seconds")

    args = parser.parse_args()

    raise SystemExit(
        run_all_tests(
            port=args.port,
            baud=args.baud,
            timeout=args.timeout,
            gap=args.gap,
        )
    )


if __name__ == "__main__":
    main()