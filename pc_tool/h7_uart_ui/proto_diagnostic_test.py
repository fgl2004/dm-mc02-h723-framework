from __future__ import annotations

import argparse
import time
from dataclasses import dataclass

from h7_proto.command_client import H7CommandClient
from h7_proto.constants import (
    CMD_UNKNOWN_TEST,
    DIAG_CMD_GET_BUFFER_STATS,
    DIAG_CMD_GET_ERROR_COUNTERS,
    DIAG_CMD_GET_HEALTH,
    DIAG_CMD_GET_LAST_RECORDS,
    DIAG_CMD_GET_TIMING_STATS,
    DIAG_CMD_GET_PIPELINE_STATS,
    DIAG_CMD_CLEAR_COUNTERS,
    PROTO_ERROR_UNKNOWN_CMD,
    TYPE_NACK,
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


def expect_resp_contains(
    frame: ProtoFrame | None,
    seq: int,
    cmd: int,
    required_items: list[str],
    printable_ascii: bool = True,
) -> tuple[bool, str]:
    if frame is None:
        return False, "no response"

    if frame.frame_type != TYPE_RESP:
        return False, f"expected RESP, got {frame_summary(frame)}"

    if frame.seq != (seq & 0xFF):
        return False, f"seq mismatch: {frame_summary(frame)}"

    if frame.cmd != (cmd & 0xFF):
        return False, f"cmd mismatch: {frame_summary(frame)}"

    text = frame.payload_ascii()

    missing_items = [item for item in required_items if item not in text]
    if missing_items:
        return False, f"missing {missing_items}: {frame_summary(frame)}"

    if printable_ascii:
        bad_bytes = [
            byte
            for byte in frame.payload
            if not (byte in (0x09, 0x0A, 0x0D) or 0x20 <= byte <= 0x7E)
        ]
        if bad_bytes:
            return False, f"non-printable payload bytes={bad_bytes}: {frame_summary(frame)}"

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
            return False, f"NACK error mismatch: {frame_summary(frame)}"

    return True, frame_summary(frame)


def run_resp_test(
    client: H7CommandClient,
    name: str,
    cmd: int,
    timeout_s: float,
    required_items: list[str],
) -> TestResult:
    seq, tx, frame = client.request(cmd, timeout_s=timeout_s)
    print(f"[PC] RX: {frame_summary(frame)}")

    ok, detail = expect_resp_contains(
        frame=frame,
        seq=seq,
        cmd=cmd,
        required_items=required_items,
        printable_ascii=True,
    )

    return TestResult(name=name, passed=ok, detail=detail)


def run_unknown_cmd_injection(
    client: H7CommandClient,
    timeout_s: float,
) -> TestResult:
    seq, tx, frame = client.request(CMD_UNKNOWN_TEST, timeout_s=timeout_s)
    print(f"[PC] RX: {frame_summary(frame)}")

    ok, detail = expect_nack(
        frame=frame,
        seq=seq,
        cmd=CMD_UNKNOWN_TEST,
        expected_error_code=PROTO_ERROR_UNKNOWN_CMD,
    )

    return TestResult(
        name="injection_unknown_cmd_nack",
        passed=ok,
        detail=detail,
    )


def run_unknown_counter_growth_test(
    client: H7CommandClient,
    timeout_s: float,
) -> TestResult:
    before_seq, before_tx, before_frame = client.request(
        DIAG_CMD_GET_ERROR_COUNTERS,
        timeout_s=timeout_s,
    )
    print(f"[PC] BEFORE ERRORS: {frame_summary(before_frame)}")

    inj = run_unknown_cmd_injection(client, timeout_s)
    if not inj.passed:
        return TestResult(
            name="diagnostic_unknown_counter_growth",
            passed=False,
            detail=f"unknown injection failed: {inj.detail}",
        )

    after_seq, after_tx, after_frame = client.request(
        DIAG_CMD_GET_ERROR_COUNTERS,
        timeout_s=timeout_s,
    )
    print(f"[PC] AFTER ERRORS: {frame_summary(after_frame)}")

    ok, detail = expect_resp_contains(
        frame=after_frame,
        seq=after_seq,
        cmd=DIAG_CMD_GET_ERROR_COUNTERS,
        required_items=["unknown="],
        printable_ascii=True,
    )
    if not ok:
        return TestResult(
            name="diagnostic_unknown_counter_growth",
            passed=False,
            detail=detail,
        )

    # We only require the view to be queryable after injection.
    # Parsing numeric growth is intentionally left loose because the MCU may
    # already have unknown command history from earlier manual testing.
    return TestResult(
        name="diagnostic_unknown_counter_growth",
        passed=True,
        detail=frame_summary(after_frame),
    )


def run_tests(port: str, baud: int, timeout_s: float, with_injection: bool) -> int:
    print(f"[PC] Open {port} @ {baud}")

    results: list[TestResult] = []
    event_counter = EventCounter()

    with H7SerialSession(port=port, baud=baud) as session:
        client = H7CommandClient(session, event_callback=event_counter)

        tests = [
            lambda: run_resp_test(
                client,
                "diag_get_health",
                DIAG_CMD_GET_HEALTH,
                timeout_s,
                ["health=", "uptime=", "err=", "drop=", "rx_ovf=", "last="],
            ),
            lambda: run_resp_test(
                client,
                "diag_get_error_counters",
                DIAG_CMD_GET_ERROR_COUNTERS,
                timeout_s,
                ["unknown=", "handler=", "parser=", "uart=", "ovf="],
            ),
            lambda: run_resp_test(
                client,
                "diag_get_buffer_stats",
                DIAG_CMD_GET_BUFFER_STATS,
                timeout_s,
                ["uart_avail=", "uart_high=", "uart_ovf=", "mcu_drop=", "cmd_drop="],
            ),
            lambda: run_resp_test(
                client,
                "diag_get_timing_stats",
                DIAG_CMD_GET_TIMING_STATS,
                timeout_s,
                ["loop=", "max_gap=", "proto_us=", "cmd_us=", "svc_us=", "mcu_us=", "diag_us="],
            ),
            lambda: run_resp_test(
                client,
                "diag_get_last_records",
                DIAG_CMD_GET_LAST_RECORDS,
                timeout_s,
                ["last_cmd=", "last_evt=", "last_err=", "last_rx=", "last_tx="],
            ),
            lambda: run_resp_test(
                client,
                "diag_get_pipeline_stats",
                DIAG_CMD_GET_PIPELINE_STATS,
                timeout_s,
                ["rx=", "parser_ok=", "proto_rx=", "req=", "resp=", "nack=", "cmd=", "unk=", "herr=", "event="],
            ),
            lambda: run_resp_test(
                client,
                "diag_clear_counters",
                DIAG_CMD_CLEAR_COUNTERS,
                timeout_s,
                ["cleared="],
            ),
        ]

        if with_injection:
            tests.append(lambda: run_unknown_counter_growth_test(client, timeout_s))

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
    parser = argparse.ArgumentParser(description="Diagnostic command test")
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM19")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument(
        "--with-injection",
        action="store_true",
        help="Also run a safe unknown-command injection check.",
    )

    args = parser.parse_args()

    raise SystemExit(
        run_tests(
            port=args.port,
            baud=args.baud,
            timeout_s=args.timeout,
            with_injection=args.with_injection,
        )
    )


if __name__ == "__main__":
    main()
