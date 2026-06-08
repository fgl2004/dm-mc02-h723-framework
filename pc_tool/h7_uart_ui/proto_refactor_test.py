from __future__ import annotations

import argparse
import time

from h7_proto.block_client import (
    block_packet_summary,
    build_block_begin_frame,
    build_block_chunk_frame,
    build_block_end_frame,
    decode_block_packet,
)
from h7_proto.constants import (
    BLOCK_MANAGER_DATA_CMD,
    MCU_INFO_CMD_GET_VERSION,
    STREAM_MANAGER_DATA_CMD,
    TYPE_ACK,
    TYPE_DATA,
    TYPE_EVENT,
    TYPE_NACK,
    TYPE_RESP,
    TYPE_WINDOW_ACK,
)
from h7_proto.event import decode_mcu_info_event, event_summary
from h7_proto.frame import frame_summary
from h7_proto.serial_session import H7SerialSession
from h7_proto.stream_client import decode_stream_sample, stream_sample_summary


def next_seq(seq: int) -> int:
    seq = (seq + 1) & 0xFF
    return 1 if seq == 0 else seq


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--listen", type=float, default=3.0)
    parser.add_argument("--send-block-demo", action="store_true")
    args = parser.parse_args()

    seq = 0

    with H7SerialSession(args.port, args.baud) as session:
        seq = next_seq(seq)
        tx = session.write_request(seq, MCU_INFO_CMD_GET_VERSION)
        print(f"[TX] GET_VERSION seq={seq}: {tx.hex(' ').upper()}")

        if args.send_block_demo:
            seq = next_seq(seq)
            session.write_raw(build_block_begin_frame(seq, handle=1, total_size=32, crc32=0))
            print(f"[TX] BLOCK BEGIN seq={seq}")

            seq = next_seq(seq)
            session.write_raw(
                build_block_chunk_frame(
                    seq,
                    handle=1,
                    offset=0,
                    chunk=bytes(range(32)),
                    need_ack=True,
                    more=False,
                )
            )
            print(f"[TX] BLOCK CHUNK seq={seq}")

            seq = next_seq(seq)
            session.write_raw(build_block_end_frame(seq, handle=1, crc32=0))
            print(f"[TX] BLOCK END seq={seq}")

        deadline = time.time() + args.listen

        while time.time() < deadline:
            frames = session.read_available_frames()

            for frame in frames:
                print("[RX]", frame_summary(frame))

                if frame.frame_type == TYPE_EVENT:
                    print("     ", event_summary(decode_mcu_info_event(frame)))

                elif frame.frame_type == TYPE_DATA and frame.cmd == STREAM_MANAGER_DATA_CMD:
                    print("     ", stream_sample_summary(decode_stream_sample(frame)))

                elif frame.frame_type in (TYPE_DATA, TYPE_ACK, TYPE_WINDOW_ACK) and frame.cmd == BLOCK_MANAGER_DATA_CMD:
                    print("     ", block_packet_summary(decode_block_packet(frame)))

                elif frame.frame_type in (TYPE_RESP, TYPE_NACK):
                    print("     RESP/NACK")

            time.sleep(0.002)


if __name__ == "__main__":
    main()
