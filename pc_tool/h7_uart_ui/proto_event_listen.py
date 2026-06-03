from __future__ import annotations

import argparse
import time

from h7_proto.constants import TYPE_EVENT, event_name
from h7_proto.event import decode_mcu_info_event, event_summary
from h7_proto.frame import frame_summary, frame_type_name
from h7_proto.serial_session import H7SerialSession


def listen_events(
    port: str,
    baud: int,
    timeout_s: float,
    max_events: int,
    show_all_frames: bool,
) -> int:
    event_count = 0
    frame_count = 0
    start_time = time.time()

    print(f"[PC] Open {port} @ {baud}")
    print("[PC] Listening for EVENT frames...")
    print("[PC] Press Ctrl+C to stop.")
    print("-" * 80)

    with H7SerialSession(port=port, baud=baud) as session:
        while True:
            if timeout_s > 0 and (time.time() - start_time) > timeout_s:
                print("[PC] Listen timeout")
                break

            frames = session.read_available_frames()

            for frame in frames:
                frame_count += 1

                if frame.frame_type == TYPE_EVENT:
                    event_count += 1
                    event = decode_mcu_info_event(frame)

                    print(
                        f"[EVENT] seq={frame.seq}, "
                        f"cmd=0x{frame.cmd:02X}({event_name(frame.cmd)}), "
                        f"len={len(frame.payload)}"
                    )
                    print(f"[EVENT] {event_summary(event)}")
                    print("-" * 80)

                    if max_events > 0 and event_count >= max_events:
                        print(f"[PC] Reached max events: {max_events}")
                        print(
                            f"[SUMMARY] frame_count={frame_count}, "
                            f"event_count={event_count}"
                        )
                        return 0

                elif show_all_frames:
                    print(
                        f"[FRAME] type={frame_type_name(frame.frame_type)}, "
                        f"{frame_summary(frame)}"
                    )
                    print("-" * 80)

            time.sleep(0.003)

    print(f"[SUMMARY] frame_count={frame_count}, event_count={event_count}")

    return 0 if event_count > 0 else 1


def main() -> None:
    parser = argparse.ArgumentParser(description="Listen for DM-MC02 H723 UART EVENT frames")
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM19")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10.0, help="Listen timeout in seconds, 0 means forever")
    parser.add_argument("--max-events", type=int, default=1, help="Stop after receiving this many events, 0 means unlimited")
    parser.add_argument("--show-all-frames", action="store_true", help="Print non-EVENT frames too")

    args = parser.parse_args()

    raise SystemExit(
        listen_events(
            port=args.port,
            baud=args.baud,
            timeout_s=args.timeout,
            max_events=args.max_events,
            show_all_frames=args.show_all_frames,
        )
    )


if __name__ == "__main__":
    main()