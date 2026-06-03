from __future__ import annotations

import argparse
import time
from dataclasses import dataclass

from h7_proto.command_client import H7CommandClient
from h7_proto.constants import (
    CMD_UNKNOWN_TEST,
    MCU_INFO_CMD_GET_APP_STATS,
    MCU_INFO_CMD_GET_FAULT_INFO,
    MCU_INFO_CMD_GET_RESET_INFO,
    MCU_INFO_CMD_GET_STATUS,
    MCU_INFO_CMD_GET_TIME_INFO,
    MCU_INFO_CMD_GET_UART_STATS,
    MCU_INFO_CMD_GET_VERSION,
    MCU_INFO_CMD_PING,
    PROTO_ERROR_UNKNOWN_CMD,
    TYPE_NACK,
    TYPE_RESP,
)
from h7_proto.event import decode_mcu_info_event, event_summary
from h7_proto.frame import ProtoFrame, frame_summary
from h7_proto.serial_session import H7SerialSession


# New command after CommandService refactor.
# Keep it local here so this test can run even if h7_proto.constants.py
# has not been updated yet.
MCU_INFO_CMD_GET_COMMAND_STATS = 0x0D


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
    expected_payload: bytes | None = None,
    expected_ascii_prefix: str | None = None,
    expected_ascii_contains: str | None = None,
    expected_ascii_all_contains: list[str] | None = None,
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

    text = frame.payload_ascii()

    if expected_ascii_prefix is not None and not text.startswith(expected_ascii_prefix):
        return False, f"ascii prefix mismatch: {frame_summary(frame)}"

    if expected_ascii_contains is not None and expected_ascii_contains not in text:
        return False, f"ascii contains mismatch: {frame_summary(frame)}"

    if expected_ascii_all_contains is not None:
        missing_items = [item for item in expected_ascii_all_contains if item not in text]
        if missing_items:
            return (
                False,
                f"ascii missing {missing_items}: {frame_summary(frame)}",
            )

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


def run_request_test(
    client: H7CommandClient,
    name: str,
    cmd: int,
    timeout_s: float,
    expect_kind: str,
    expected_payload: bytes | None = None,
    expected_ascii_prefix: str | None = None,
    expected_ascii_contains: str | None = None,
    expected_ascii_all_contains: list[str] | None = None,
    expected_error_code: int | None = None,
) -> TestResult:
    seq, tx, frame = client.request(cmd, timeout_s=timeout_s)

    print(f"[PC] RX: {frame_summary(frame)}")

    if expect_kind == "resp":
        ok, detail = expect_resp(
            frame,
            seq,
            cmd,
            expected_payload=expected_payload,
            expected_ascii_prefix=expected_ascii_prefix,
            expected_ascii_contains=expected_ascii_contains,
            expected_ascii_all_contains=expected_ascii_all_contains,
        )
    elif expect_kind == "nack":
        ok, detail = expect_nack(
            frame,
            seq,
            cmd,
            expected_error_code=expected_error_code,
        )
    else:
        ok, detail = False, f"invalid expect_kind={expect_kind}"

    return TestResult(name, ok, detail)


def run_tests(port: str, baud: int, timeout_s: float) -> int:
    print(f"[PC] Open {port} @ {baud}")

    results: list[TestResult] = []
    event_counter = EventCounter()

    with H7SerialSession(port=port, baud=baud) as session:
        client = H7CommandClient(session, event_callback=event_counter)

        tests = [
            lambda: run_request_test(
                client,
                "command_ping",
                MCU_INFO_CMD_PING,
                timeout_s,
                "resp",
                expected_payload=b"PONG",
            ),
            lambda: run_request_test(
                client,
                "command_get_version",
                MCU_INFO_CMD_GET_VERSION,
                timeout_s,
                "resp",
                expected_ascii_prefix="DM-MC02-H723",
            ),
            lambda: run_request_test(
                client,
                "command_get_status",
                MCU_INFO_CMD_GET_STATUS,
                timeout_s,
                "resp",
                expected_payload=b"OK",
            ),
            lambda: run_request_test(
                client,
                "command_get_time_info",
                MCU_INFO_CMD_GET_TIME_INFO,
                timeout_s,
                "resp",
                expected_ascii_prefix="tick=",
            ),
            lambda: run_request_test(
                client,
                "command_get_uart_stats",
                MCU_INFO_CMD_GET_UART_STATS,
                timeout_s,
                "resp",
                expected_ascii_contains="rx=",
            ),
            lambda: run_request_test(
                client,
                "command_get_app_stats",
                MCU_INFO_CMD_GET_APP_STATS,
                timeout_s,
                "resp",
                expected_ascii_contains="run=",
            ),
            lambda: run_request_test(
                client,
                "command_get_command_stats",
                MCU_INFO_CMD_GET_COMMAND_STATS,
                timeout_s,
                "resp",
                expected_ascii_all_contains=["init=", "reg=", "disp="],
            ),
            lambda: run_request_test(
                client,
                "command_get_reset_info",
                MCU_INFO_CMD_GET_RESET_INFO,
                timeout_s,
                "resp",
                expected_ascii_contains="cause=",
            ),
            lambda: run_request_test(
                client,
                "command_get_fault_info_currently_nack",
                MCU_INFO_CMD_GET_FAULT_INFO,
                timeout_s,
                "nack",
                expected_error_code=PROTO_ERROR_UNKNOWN_CMD,
            ),
            lambda: run_request_test(
                client,
                "command_unknown_nack",
                CMD_UNKNOWN_TEST,
                timeout_s,
                "nack",
                expected_error_code=PROTO_ERROR_UNKNOWN_CMD,
            ),
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
    print(
        f"[SUMMARY] total={len(results)}, pass={pass_count}, "
        f"fail={fail_count}, event_seen={event_counter.count}"
    )

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
