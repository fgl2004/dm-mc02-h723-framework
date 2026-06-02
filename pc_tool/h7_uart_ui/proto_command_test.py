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

MCU_INFO_CMD_PING = 0x01
MCU_INFO_CMD_GET_VERSION = 0x02
MCU_INFO_CMD_GET_STATUS = 0x03
MCU_INFO_CMD_GET_RESET_INFO = 0x04
MCU_INFO_CMD_GET_TIME_INFO = 0x05
MCU_INFO_CMD_GET_FAULT_INFO = 0x06
MCU_INFO_CMD_GET_UART_STATS = 0x07
MCU_INFO_CMD_GET_APP_STATS = 0x08

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

    crc = crc16_ccitt_false(header + payload)

    return bytes([SOF1, SOF2]) + header + payload + bytes(
        [
            crc & 0xFF,
            (crc >> 8) & 0xFF,
        ]
    )


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

    recv_crc = frame_bytes[-2] | (frame_bytes[-1] << 8)
    calc_crc = crc16_ccitt_false(frame_bytes[2:-2])

    if recv_crc != calc_crc:
        print(f"[PC] CRC error: recv=0x{recv_crc:04X}, calc=0x{calc_crc:04X}")
        return None

    return ProtoFrame(
        frame_type=frame_type,
        flags=flags,
        seq=seq,
        cmd=cmd,
        payload=frame_bytes[9:-2],
    )


def flush_serial(ser: serial.Serial) -> None:
    time.sleep(0.05)

    try:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
    except Exception:
        pass


def read_one_frame(ser: serial.Serial, timeout_s: float) -> ProtoFrame | None:
    deadline = time.time() + timeout_s
    rx_buf = bytearray()

    while time.time() < deadline:
        n = ser.in_waiting

        if n > 0:
            rx_buf.extend(ser.read(n))

            frame = try_parse_one_frame(rx_buf)
            if frame is not None:
                return frame

        time.sleep(0.003)

    return None


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


def send_req_and_read(
    ser: serial.Serial,
    cmd: int,
    seq: int,
    timeout_s: float,
    payload: bytes = b"",
) -> ProtoFrame | None:
    tx = build_frame(TYPE_REQ, 0, seq, cmd, payload)

    print(f"[PC] TX cmd=0x{cmd:02X}, seq={seq}: {tx.hex(' ').upper()}")

    flush_serial(ser)
    ser.write(tx)

    frame = read_one_frame(ser, timeout_s)

    print(f"[PC] RX: {frame_summary(frame)}")

    return frame


def expect_resp(
    frame: ProtoFrame | None,
    seq: int,
    cmd: int,
    expected_payload: bytes | None = None,
    expected_ascii_prefix: str | None = None,
    expected_ascii_contains: str | None = None,
) -> tuple[bool, str]:
    if frame is None:
        return False, "no response"

    if frame.frame_type != TYPE_RESP:
        return False, f"expected RESP, got {frame_summary(frame)}"

    if frame.seq != (seq & 0xFF):
        return False, f"seq mismatch: {frame_summary(frame)}"

    if frame.cmd != (cmd & 0xFF):
        return False, f"cmd mismatch: {frame_summary(frame)}"

    if expected_payload is not None and frame.payload != expected_payload:
        return False, f"payload mismatch: {frame_summary(frame)}"

    text = frame.payload.decode("utf-8", errors="replace")

    if expected_ascii_prefix is not None and not text.startswith(expected_ascii_prefix):
        return False, f"ascii prefix mismatch: {frame_summary(frame)}"

    if expected_ascii_contains is not None and expected_ascii_contains not in text:
        return False, f"ascii contains mismatch: {frame_summary(frame)}"

    return True, frame_summary(frame)


def expect_nack(
    frame: ProtoFrame | None,
    seq: int,
    cmd: int,
    expected_error_code: int | None = None,
) -> tuple[bool, str]:
    if frame is None:
        return False, "no response"

    if frame.frame_type != TYPE_NACK:
        return False, f"expected NACK, got {frame_summary(frame)}"

    if frame.seq != (seq & 0xFF):
        return False, f"seq mismatch: {frame_summary(frame)}"

    if frame.cmd != (cmd & 0xFF):
        return False, f"cmd mismatch: {frame_summary(frame)}"

    if expected_error_code is not None:
        if len(frame.payload) < 1:
            return False, f"NACK payload too short: {frame_summary(frame)}"

        if frame.payload[0] != (expected_error_code & 0xFF):
            return False, f"NACK error code mismatch: {frame_summary(frame)}"

    return True, frame_summary(frame)


def test_ping(ser: serial.Serial, timeout_s: float) -> TestResult:
    name = "command_ping"
    seq = 1
    frame = send_req_and_read(ser, MCU_INFO_CMD_PING, seq, timeout_s)

    ok, detail = expect_resp(frame, seq, MCU_INFO_CMD_PING, expected_payload=b"PONG")
    return TestResult(name, ok, detail)


def test_version(ser: serial.Serial, timeout_s: float) -> TestResult:
    name = "command_get_version"
    seq = 2
    frame = send_req_and_read(ser, MCU_INFO_CMD_GET_VERSION, seq, timeout_s)

    ok, detail = expect_resp(
        frame,
        seq,
        MCU_INFO_CMD_GET_VERSION,
        expected_ascii_prefix="DM-MC02-H723",
    )

    return TestResult(name, ok, detail)


def test_status(ser: serial.Serial, timeout_s: float) -> TestResult:
    name = "command_get_status"
    seq = 3
    frame = send_req_and_read(ser, MCU_INFO_CMD_GET_STATUS, seq, timeout_s)

    ok, detail = expect_resp(frame, seq, MCU_INFO_CMD_GET_STATUS, expected_payload=b"OK")
    return TestResult(name, ok, detail)


def test_time_info(ser: serial.Serial, timeout_s: float) -> TestResult:
    name = "command_get_time_info"
    seq = 4
    frame = send_req_and_read(ser, MCU_INFO_CMD_GET_TIME_INFO, seq, timeout_s)

    ok, detail = expect_resp(
        frame,
        seq,
        MCU_INFO_CMD_GET_TIME_INFO,
        expected_ascii_prefix="tick=",
    )

    return TestResult(name, ok, detail)


def test_uart_stats(ser: serial.Serial, timeout_s: float) -> TestResult:
    name = "command_get_uart_stats"
    seq = 5
    frame = send_req_and_read(ser, MCU_INFO_CMD_GET_UART_STATS, seq, timeout_s)

    ok, detail = expect_resp(
        frame,
        seq,
        MCU_INFO_CMD_GET_UART_STATS,
        expected_ascii_contains="rx=",
    )

    if not ok:
        return TestResult(name, ok, detail)

    text = frame.payload.decode("utf-8", errors="replace")
    if "avail=" not in text or "free=" not in text:
        return TestResult(name, False, f"missing uart fields: {frame_summary(frame)}")

    return TestResult(name, True, detail)


def test_app_stats(ser: serial.Serial, timeout_s: float) -> TestResult:
    name = "command_get_app_stats"
    seq = 6
    frame = send_req_and_read(ser, MCU_INFO_CMD_GET_APP_STATS, seq, timeout_s)

    ok, detail = expect_resp(
        frame,
        seq,
        MCU_INFO_CMD_GET_APP_STATS,
        expected_ascii_contains="run=",
    )

    if not ok:
        return TestResult(name, ok, detail)

    text = frame.payload.decode("utf-8", errors="replace")
    if "post=" not in text or "fwd=" not in text or "drop=" not in text:
        return TestResult(name, False, f"missing app stat fields: {frame_summary(frame)}")

    return TestResult(name, True, detail)


def test_reset_info_currently_nack(ser: serial.Serial, timeout_s: float) -> TestResult:
    name = "command_get_reset_info_currently_nack"
    seq = 7
    frame = send_req_and_read(ser, MCU_INFO_CMD_GET_RESET_INFO, seq, timeout_s)

    ok, detail = expect_nack(
        frame,
        seq,
        MCU_INFO_CMD_GET_RESET_INFO,
        expected_error_code=PROTO_ERROR_UNKNOWN_CMD,
    )

    return TestResult(name, ok, detail)


def test_fault_info_currently_nack(ser: serial.Serial, timeout_s: float) -> TestResult:
    name = "command_get_fault_info_currently_nack"
    seq = 8
    frame = send_req_and_read(ser, MCU_INFO_CMD_GET_FAULT_INFO, seq, timeout_s)

    ok, detail = expect_nack(
        frame,
        seq,
        MCU_INFO_CMD_GET_FAULT_INFO,
        expected_error_code=PROTO_ERROR_UNKNOWN_CMD,
    )

    return TestResult(name, ok, detail)


def test_unknown_cmd_nack(ser: serial.Serial, timeout_s: float) -> TestResult:
    name = "command_unknown_nack"
    seq = 9
    frame = send_req_and_read(ser, CMD_UNKNOWN_TEST, seq, timeout_s)

    ok, detail = expect_nack(
        frame,
        seq,
        CMD_UNKNOWN_TEST,
        expected_error_code=PROTO_ERROR_UNKNOWN_CMD,
    )

    return TestResult(name, ok, detail)


def run_tests(port: str, baud: int, timeout_s: float) -> int:
    print(f"[PC] Open {port} @ {baud}")

    results: list[TestResult] = []

    with serial.Serial(port=port, baudrate=baud, timeout=0.02, write_timeout=0.5) as ser:
        time.sleep(0.2)
        flush_serial(ser)

        tests = [
            lambda: test_ping(ser, timeout_s),
            lambda: test_version(ser, timeout_s),
            lambda: test_status(ser, timeout_s),
            lambda: test_time_info(ser, timeout_s),
            lambda: test_uart_stats(ser, timeout_s),
            lambda: test_app_stats(ser, timeout_s),
            lambda: test_reset_info_currently_nack(ser, timeout_s),
            lambda: test_fault_info_currently_nack(ser, timeout_s),
            lambda: test_unknown_cmd_nack(ser, timeout_s),
        ]

        for test_fn in tests:
            result = test_fn()
            results.append(result)

            status = "PASS" if result.passed else "FAIL"
            print(f"[RESULT] {status}: {result.name} - {result.detail}")
            print("-" * 80)

            time.sleep(0.1)

    pass_count = sum(1 for item in results if item.passed)
    fail_count = len(results) - pass_count

    print("=" * 80)
    print(f"[SUMMARY] total={len(results)}, pass={pass_count}, fail={fail_count}")

    for item in results:
        status = "PASS" if item.passed else "FAIL"
        print(f"[SUMMARY] {status}: {item.name} - {item.detail}")

    print("=" * 80)

    return 0 if fail_count == 0 else 1


def main() -> None:
    parser = argparse.ArgumentParser(description="McuInfoApp command test")
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM19")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=1.0)

    args = parser.parse_args()

    raise SystemExit(run_tests(args.port, args.baud, args.timeout))


if __name__ == "__main__":
    main()