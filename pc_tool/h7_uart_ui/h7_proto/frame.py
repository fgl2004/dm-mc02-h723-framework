from __future__ import annotations

from dataclasses import dataclass

from .constants import (
    FRAME_TYPE_NAME_MAP,
    TYPE_ACK,
    TYPE_DATA,
    TYPE_EVENT,
    TYPE_NACK,
    TYPE_RESP,
    TYPE_WINDOW_ACK,
)


@dataclass
class ProtoFrame:
    frame_type: int
    flags: int
    seq: int
    cmd: int
    payload: bytes

    def is_resp(self) -> bool:
        return self.frame_type == TYPE_RESP

    def is_nack(self) -> bool:
        return self.frame_type == TYPE_NACK

    def is_event(self) -> bool:
        return self.frame_type == TYPE_EVENT

    def is_data(self) -> bool:
        return self.frame_type == TYPE_DATA

    def is_ack(self) -> bool:
        return self.frame_type == TYPE_ACK

    def is_window_ack(self) -> bool:
        return self.frame_type == TYPE_WINDOW_ACK

    def is_response_for(self, seq: int, cmd: int) -> bool:
        return (
            self.frame_type in (TYPE_RESP, TYPE_NACK)
            and self.seq == (seq & 0xFF)
            and self.cmd == (cmd & 0xFF)
        )

    def payload_ascii(self) -> str:
        return self.payload.decode("utf-8", errors="replace")

    def payload_hex(self) -> str:
        return self.payload.hex(" ").upper()


def frame_type_name(frame_type: int) -> str:
    return FRAME_TYPE_NAME_MAP.get(frame_type, f"UNKNOWN_0x{frame_type:02X}")


def frame_summary(frame: ProtoFrame | None) -> str:
    if frame is None:
        return "None"

    return (
        f"type=0x{frame.frame_type:02X}({frame_type_name(frame.frame_type)}), "
        f"flags=0x{frame.flags:02X}, "
        f"seq={frame.seq}, "
        f"cmd=0x{frame.cmd:02X}, "
        f"len={len(frame.payload)}, "
        f"payload_hex=[{frame.payload_hex()}], "
        f"payload_ascii=[{frame.payload_ascii()}]"
    )
