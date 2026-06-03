from __future__ import annotations

import argparse

from h7_proto.command_client import H7CommandClient
from h7_proto.constants import (
    MCU_INFO_CMD_GET_STATUS,
    MCU_INFO_CMD_GET_VERSION,
    MCU_INFO_CMD_PING,
    TYPE_NACK,
    TYPE_RESP,
)
from h7_proto.frame import frame_summary
from h7_proto.serial_session import H7SerialSession


CMD_MAP = {
    "ping": MCU_INFO_CMD_PING,
    "version": MCU_INFO_CMD_GET_VERSION,
    "status": MCU_INFO_CMD_GET_STATUS,
}


def send_command(port: str, baud: int, cmd: int, timeout: float) -> int:
    print(f"[PC] Open {port} @ {baud}")

    with H7SerialSession(port=port, baud=baud) as session:
        client = H7CommandClient(session)

        seq, tx, frame = client.request(cmd, timeout_s=timeout)

        print("[PC] TX:", tx.hex(" ").upper())

        if frame is None:
            print("[PC] RX timeout")
            return 1

        print("[PC] RX frame:", frame_summary(frame))

        if frame.payload:
            print("[PC] RX payload HEX:", frame.payload_hex())
            print("[PC] RX payload ASCII:", frame.payload_ascii())

        if frame.frame_type == TYPE_RESP:
            print("[PC] RESULT: RESP OK")
            return 0

        if frame.frame_type == TYPE_NACK:
            print("[PC] RESULT: NACK")
            return 1

        print("[PC] RESULT: unexpected frame type")
        return 1


def main() -> None:
    parser = argparse.ArgumentParser(description="DM-MC02 H723 UART protocol command test")
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM19")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--cmd", choices=["ping", "version", "status"], default="ping")
    parser.add_argument("--timeout", type=float, default=1.0)

    args = parser.parse_args()

    raise SystemExit(
        send_command(
            port=args.port,
            baud=args.baud,
            cmd=CMD_MAP[args.cmd],
            timeout=args.timeout,
        )
    )


if __name__ == "__main__":
    main()