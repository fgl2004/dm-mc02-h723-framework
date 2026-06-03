from __future__ import annotations

import argparse
import time
from dataclasses import dataclass

from h7_proto.codec import (
    build_frame,
    build_frame_with_raw_len,
    corrupt_last_crc_byte,
)
from h7_proto.constants import (
    CMD_UNKNOWN_TEST,
    MAX_PAYLOAD,
    MCU_INFO_CMD_GET_STATUS,
    MCU_INFO_CMD_GET_VERSION,
    MCU_INFO_CMD_PING,
    PROTO_ERROR_UNKNOWN_CMD,
    TYPE_NACK,
    TYPE_REQ,
    TYPE_RESP,
)
from h7_proto.event import decode_mcu_info_event, event_summary
from h7_proto.frame import ProtoFrame, frame_summary
from h7_proto.serial_session import H7SerialSession


@dataclass
class TestResult:
    name: str
    passed: bool
    detail: str


class EventCounter:
    def __init__(self) -> None:
        self.count = 0

    def __call__(self, frame: ProtoFrame) -> None:
        self.count += 1
        event = decode_mcu_info_event(frame)
        print(f"[EVENT] {event_summary(event)}")


def expect_resp(
    frame: ProtoFrame | None,
    seq: int,
    cmd: int,
    payload: bytes | None = None,
    ascii_prefix: str | None = None,
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

    if ascii_prefix is not None:
        text = frame.payload_ascii()

        if not text.startswith(ascii_prefix):
            return False, f"unexpected ascii prefix: {frame_summary(frame)}"

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


def read_target_response(
    session: H7SerialSession,
    seq: int,
    cmd: int,
    timeout_s: float,
    event_counter: EventCounter,
) -> ProtoFrame | None:
    return session.read_until(
        predicate=lambda f: f.is_response_for(seq, cmd),
        timeout_s=timeout_s,
        event_callback=event_counter,
        show_ignored_frames=False,
    )


def read_target_responses(
    session: H7SerialSession,
    targets: list[tuple[int, int]],
    timeout_s: float,
    event_counter: EventCounter,
) -> list[ProtoFrame]:
    """
    Read until all target (seq, cmd) responses are collected.

    EVENT frames are ignored and counted.
    Other unrelated frames are ignored.
    """
    deadline = time.time() + timeout_s
    found: list[ProtoFrame] = []
    pending = list(targets)

    while time.time() < deadline and pending:
        frames = session.read_available_frames()

        for frame in frames:
            if frame.is_event():
                event_counter(frame)
                continue

            matched_index = None

            for i, (seq, cmd) in enumerate(pending):
                if frame.is_response_for(seq, cmd):
                    matched_index = i
                    break

            if matched_index is not None:
                found.append(frame)
                del pending[matched_index]
            else:
                print(f"[PC] ignored non-target frame: {frame_summary(frame)}")

        time.sleep(0.003)

    return found


def expect_no_target_response(
    session: H7SerialSession,
    seq: int,
    cmd: int,
    timeout_s: float,
    event_counter: EventCounter,
) -> tuple[bool, str]:
    """
    Pass if no RESP/NACK matching the given seq/cmd is received.

    EVENT frames are allowed and ignored.
    """
    frame = read_target_response(
        session=session,
        seq=seq,
        cmd=cmd,
        timeout_s=timeout_s,
        event_counter=event_counter,
    )

    if frame is None:
        return True, "no target response as expected"

    return False, f"unexpected target response: {frame_summary(frame)}"


def test_ping(session: H7SerialSession, timeout: float, event_counter: EventCounter) -> TestResult:
    name = "valid_ping"
    seq = 1
    tx = build_frame(TYPE_REQ, 0, seq, MCU_INFO_CMD_PING)

    session.flush()
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    session.write_raw(tx)

    frame = read_target_response(session, seq, MCU_INFO_CMD_PING, timeout, event_counter)

    ok, detail = expect_resp(frame, seq, MCU_INFO_CMD_PING, b"PONG")
    return TestResult(name, ok, detail)


def test_get_version(session: H7SerialSession, timeout: float, event_counter: EventCounter) -> TestResult:
    name = "get_version"
    seq = 2
    tx = build_frame(TYPE_REQ, 0, seq, MCU_INFO_CMD_GET_VERSION)

    session.flush()
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    session.write_raw(tx)

    frame = read_target_response(session, seq, MCU_INFO_CMD_GET_VERSION, timeout, event_counter)

    ok, detail = expect_resp(
        frame,
        seq,
        MCU_INFO_CMD_GET_VERSION,
        ascii_prefix="DM-MC02-H723",
    )

    return TestResult(name, ok, detail)


def test_get_status(session: H7SerialSession, timeout: float, event_counter: EventCounter) -> TestResult:
    name = "get_status"
    seq = 3
    tx = build_frame(TYPE_REQ, 0, seq, MCU_INFO_CMD_GET_STATUS)

    session.flush()
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    session.write_raw(tx)

    frame = read_target_response(session, seq, MCU_INFO_CMD_GET_STATUS, timeout, event_counter)

    ok, detail = expect_resp(frame, seq, MCU_INFO_CMD_GET_STATUS, b"OK")
    return TestResult(name, ok, detail)


def test_unknown_cmd_nack(session: H7SerialSession, timeout: float, event_counter: EventCounter) -> TestResult:
    name = "unknown_cmd_nack"
    seq = 4
    tx = build_frame(TYPE_REQ, 0, seq, CMD_UNKNOWN_TEST)

    session.flush()
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    session.write_raw(tx)

    frame = read_target_response(session, seq, CMD_UNKNOWN_TEST, timeout, event_counter)

    ok, detail = expect_nack(frame, seq, CMD_UNKNOWN_TEST, PROTO_ERROR_UNKNOWN_CMD)
    return TestResult(name, ok, detail)


def test_crc_error_no_response(
    session: H7SerialSession,
    timeout: float,
    event_counter: EventCounter,
) -> TestResult:
    name = "crc_error_no_response"
    seq = 5
    good = build_frame(TYPE_REQ, 0, seq, MCU_INFO_CMD_PING)
    bad = corrupt_last_crc_byte(good)

    session.flush()
    print(f"[TEST] {name} TX:", bad.hex(" ").upper())
    session.write_raw(bad)

    ok, detail = expect_no_target_response(
        session=session,
        seq=seq,
        cmd=MCU_INFO_CMD_PING,
        timeout_s=timeout,
        event_counter=event_counter,
    )

    return TestResult(name, ok, detail)


def test_garbage_before_valid_frame(
    session: H7SerialSession,
    timeout: float,
    event_counter: EventCounter,
) -> TestResult:
    name = "garbage_before_valid_frame"
    seq = 6
    garbage = bytes([0x00, 0x11, 0x22, 0x33, 0xA5, 0xA5, 0x00, 0x55])
    good = build_frame(TYPE_REQ, 0, seq, MCU_INFO_CMD_PING)
    tx = garbage + good

    session.flush()
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    session.write_raw(tx)

    frame = read_target_response(session, seq, MCU_INFO_CMD_PING, timeout, event_counter)

    ok, detail = expect_resp(frame, seq, MCU_INFO_CMD_PING, b"PONG")
    return TestResult(name, ok, detail)


def test_half_packet(
    session: H7SerialSession,
    timeout: float,
    gap_s: float,
    event_counter: EventCounter,
) -> TestResult:
    name = "half_packet"
    seq = 7
    tx = build_frame(TYPE_REQ, 0, seq, MCU_INFO_CMD_PING)

    split_pos = 5

    session.flush()
    print(f"[TEST] {name} TX part1:", tx[:split_pos].hex(" ").upper())
    session.write_raw(tx[:split_pos])

    time.sleep(gap_s)

    print(f"[TEST] {name} TX part2:", tx[split_pos:].hex(" ").upper())
    session.write_raw(tx[split_pos:])

    frame = read_target_response(session, seq, MCU_INFO_CMD_PING, timeout, event_counter)

    ok, detail = expect_resp(frame, seq, MCU_INFO_CMD_PING, b"PONG")
    return TestResult(name, ok, detail)


def test_sticky_packets(
    session: H7SerialSession,
    timeout: float,
    event_counter: EventCounter,
) -> TestResult:
    name = "sticky_packets"

    tx1 = build_frame(TYPE_REQ, 0, 8, MCU_INFO_CMD_PING)
    tx2 = build_frame(TYPE_REQ, 0, 9, MCU_INFO_CMD_GET_STATUS)
    tx3 = build_frame(TYPE_REQ, 0, 10, MCU_INFO_CMD_GET_VERSION)
    tx = tx1 + tx2 + tx3

    session.flush()
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    session.write_raw(tx)

    frames = read_target_responses(
        session=session,
        targets=[
            (8, MCU_INFO_CMD_PING),
            (9, MCU_INFO_CMD_GET_STATUS),
            (10, MCU_INFO_CMD_GET_VERSION),
        ],
        timeout_s=timeout,
        event_counter=event_counter,
    )

    if len(frames) != 3:
        return TestResult(name, False, f"expected 3 target frames, got {len(frames)}")

    checks = [
        expect_resp(frames[0], 8, MCU_INFO_CMD_PING, b"PONG"),
        expect_resp(frames[1], 9, MCU_INFO_CMD_GET_STATUS, b"OK"),
        expect_resp(frames[2], 10, MCU_INFO_CMD_GET_VERSION, ascii_prefix="DM-MC02-H723"),
    ]

    for ok, detail in checks:
        if not ok:
            return TestResult(name, False, detail)

    return TestResult(name, True, "3 sticky responses parsed")


def test_invalid_type_no_response(
    session: H7SerialSession,
    timeout: float,
    event_counter: EventCounter,
) -> TestResult:
    name = "invalid_type_no_response"
    seq = 11
    invalid_type = 0x55
    tx = build_frame(invalid_type, 0, seq, MCU_INFO_CMD_PING)

    session.flush()
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    session.write_raw(tx)

    ok, detail = expect_no_target_response(
        session=session,
        seq=seq,
        cmd=MCU_INFO_CMD_PING,
        timeout_s=timeout,
        event_counter=event_counter,
    )

    return TestResult(name, ok, detail)


def test_oversized_len_no_response(
    session: H7SerialSession,
    timeout: float,
    event_counter: EventCounter,
) -> TestResult:
    name = "oversized_len_no_response"
    seq = 12
    raw_len = MAX_PAYLOAD + 1
    tx = build_frame_with_raw_len(TYPE_REQ, 0, seq, MCU_INFO_CMD_PING, raw_len, b"")

    session.flush()
    print(f"[TEST] {name} TX:", tx.hex(" ").upper())
    session.write_raw(tx)

    ok, detail = expect_no_target_response(
        session=session,
        seq=seq,
        cmd=MCU_INFO_CMD_PING,
        timeout_s=timeout,
        event_counter=event_counter,
    )

    return TestResult(name, ok, detail)


def run_all_tests(port: str, baud: int, timeout: float, gap: float) -> int:
    results: list[TestResult] = []
    event_counter = EventCounter()

    print(f"[PC] Open {port} @ {baud}")

    with H7SerialSession(port=port, baud=baud) as session:
        tests = [
            lambda: test_ping(session, timeout, event_counter),
            lambda: test_get_version(session, timeout, event_counter),
            lambda: test_get_status(session, timeout, event_counter),
            lambda: test_unknown_cmd_nack(session, timeout, event_counter),
            lambda: test_crc_error_no_response(session, timeout, event_counter),
            lambda: test_garbage_before_valid_frame(session, timeout, event_counter),
            lambda: test_half_packet(session, timeout, gap, event_counter),
            lambda: test_sticky_packets(session, timeout, event_counter),
            lambda: test_invalid_type_no_response(session, timeout, event_counter),
            lambda: test_oversized_len_no_response(session, timeout, event_counter),
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
    print(
        f"[SUMMARY] total={len(results)}, pass={pass_count}, "
        f"fail={fail_count}, event_seen={event_counter.count}"
    )

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