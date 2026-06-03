from __future__ import annotations

from dataclasses import dataclass

from .constants import app_name, event_name
from .frame import ProtoFrame


@dataclass
class McuInfoEvent:
    event_id: int
    app_id: int
    tick_ms: int
    data: bytes

    def data_ascii(self) -> str:
        return self.data.decode("utf-8", errors="replace")

    def data_hex(self) -> str:
        return self.data.hex(" ").upper()


def decode_mcu_info_event(frame: ProtoFrame) -> McuInfoEvent | None:
    """
    Current EVENT payload format:

        byte0      app_id
        byte1      original_event_id
        byte2~5    tick_ms little-endian
        byte6..N   event payload
    """
    if not frame.is_event():
        return None

    if len(frame.payload) < 6:
        return None

    app_id = frame.payload[0]
    original_event_id = frame.payload[1]

    tick_ms = (
        frame.payload[2]
        | (frame.payload[3] << 8)
        | (frame.payload[4] << 16)
        | (frame.payload[5] << 24)
    )

    data = frame.payload[6:]

    return McuInfoEvent(
        event_id=original_event_id,
        app_id=app_id,
        tick_ms=tick_ms,
        data=data,
    )


def event_summary(event: McuInfoEvent | None) -> str:
    if event is None:
        return "invalid event"

    return (
        f"app_id={event.app_id}({app_name(event.app_id)}), "
        f"event_id=0x{event.event_id:02X}({event_name(event.event_id)}), "
        f"tick={event.tick_ms} ms, "
        f"data_hex=[{event.data_hex()}], "
        f"data_ascii=[{event.data_ascii()}]"
    )