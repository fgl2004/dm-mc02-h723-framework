from __future__ import annotations

import argparse
import time
from dataclasses import dataclass

from h7_proto.command_client import H7CommandClient
from h7_proto.constants import (
    MCU_INFO_CMD_GET_APP_STATS,
    MCU_INFO_CMD_GET_STATUS,
    MCU_INFO_CMD_GET_TIME_INFO,
    MCU_INFO_CMD_GET_UART_STATS,
    TYPE_RESP,
)
from h7_proto.event import decode_mcu_info_event, event_summary
from h7_proto.frame import ProtoFrame, frame_summary
from h7_proto.serial_session import H7SerialSession


@dataclass
class AsyncMixStats:
    cmd_sent: int = 0
    cmd_pass: int = 0
    cmd_fail: int = 0
    event_seen: int = 0
    timeout_count: int = 0
    unexpected_resp_count: int = 0


class EventCounter:
    def __init__(self) -> None:
        self.count = 0

    def __call__(self, frame: ProtoFrame) -> None:
        self.count += 1
        event = decode_mcu_info_event(frame)
        print(f"[EVENT] {event_summary(event)}")


def choose_cmd(index: int) -> int:
    """
    Rotate several read-only commands to simulate normal PC polling.
    """
    cmds = [
        MCU_INFO_CMD_GET_STATUS,
        MCU_INFO_CMD_GET_TIME_INFO,
        MCU_INFO_CMD_GET_UART_STATS,
        MCU_INFO_CMD_GET_APP_STATS,
    ]

    return cmds[index % len(cmds)]


def expected_payload_check(cmd: int, frame: ProtoFrame) -> tuple[bool, str]:
    if frame.frame_type != TYPE_RESP:
        return False, f"expected RESP, got {frame_summary(frame)}"

    text = frame.payload_ascii()

    if cmd == MCU_INFO_CMD_GET_STATUS:
        if frame.payload != b"OK":
            return False, f"expected OK, got {frame_summary(frame)}"

    elif cmd == MCU_INFO_CMD_GET_TIME_INFO:
        if not text.startswith("tick="):
            return False, f"expected tick=, got {frame_summary(frame)}"

    elif cmd == MCU_INFO_CMD_GET_UART_STATS:
        if "rx=" not in text or "avail=" not in text or "free=" not in text:
            return False, f"expected uart stats, got {frame_summary(frame)}"

    elif cmd == MCU_INFO_CMD_GET_APP_STATS:
        if "run=" not in text or "post=" not in text or "fwd=" not in text:
            return False, f"expected app stats, got {frame_summary(frame)}"

    return True, frame_summary(frame)


def run_async_mix_test(
    port: str,
    baud: int,
    count: int,
    timeout_s: float,
    interval_ms: float,
    warmup_s: float,
) -> int:
    stats = AsyncMixStats()
    event_counter = EventCounter()

    print(f"[PC] Open {port} @ {baud}")
    print(
        f"[PC] Async mix test: count={count}, "
        f"timeout={timeout_s}s, interval={interval_ms}ms, warmup={warmup_s}s"
    )
    print("-" * 80)

    with H7SerialSession(port=port, baud=baud) as session:
        client = H7CommandClient(session, event_callback=event_counter)

        if warmup_s > 0:
            print(f"[PC] Warmup read for {warmup_s}s to collect early EVENT frames...")
            session.read_until(
                predicate=lambda f: False,
                timeout_s=warmup_s,
                event_callback=event_counter,
            )
            print(f"[PC] Warmup done, event_seen={event_counter.count}")
            print("-" * 80)

        for i in range(count):
            cmd = choose_cmd(i)
            stats.cmd_sent += 1

            seq, tx, frame = client.request(
                cmd,
                timeout_s=timeout_s,
                show_tx=False,
                show_ignored_frames=False,
            )

            if frame is None:
                stats.cmd_fail += 1
                stats.timeout_count += 1
                print(f"[RESULT] FAIL index={i}, cmd=0x{cmd:02X}, seq={seq}, timeout")
            else:
                ok, detail = expected_payload_check(cmd, frame)

                if ok:
                    stats.cmd_pass += 1
                    print(
                        f"[RESULT] PASS index={i}, cmd=0x{cmd:02X}, "
                        f"seq={seq}, {detail}"
                    )
                else:
                    stats.cmd_fail += 1
                    stats.unexpected_resp_count += 1
                    print(
                        f"[RESULT] FAIL index={i}, cmd=0x{cmd:02X}, "
                        f"seq={seq}, {detail}"
                    )

            if interval_ms > 0:
                time.sleep(interval_ms / 1000.0)

    stats.event_seen = event_counter.count

    print("=" * 80)
    print(
        f"[SUMMARY] cmd_sent={stats.cmd_sent}, "
        f"cmd_pass={stats.cmd_pass}, "
        f"cmd_fail={stats.cmd_fail}, "
        f"timeout={stats.timeout_count}, "
        f"unexpected_resp={stats.unexpected_resp_count}, "
        f"event_seen={stats.event_seen}"
    )
    print("=" * 80)

    return 0 if stats.cmd_fail == 0 else 1


def main() -> None:
    parser = argparse.ArgumentParser(description="Async EVENT + command mixed test")
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM19")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--count", type=int, default=50)
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument("--interval-ms", type=float, default=20.0)
    parser.add_argument("--warmup", type=float, default=0.5)

    args = parser.parse_args()

    raise SystemExit(
        run_async_mix_test(
            port=args.port,
            baud=args.baud,
            count=args.count,
            timeout_s=args.timeout,
            interval_ms=args.interval_ms,
            warmup_s=args.warmup,
        )
    )


if __name__ == "__main__":
    main()