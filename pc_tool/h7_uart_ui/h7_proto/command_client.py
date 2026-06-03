from __future__ import annotations

from collections.abc import Callable

from .constants import (
    MCU_INFO_CMD_GET_APP_STATS,
    MCU_INFO_CMD_GET_STATUS,
    MCU_INFO_CMD_GET_TIME_INFO,
    MCU_INFO_CMD_GET_UART_STATS,
    MCU_INFO_CMD_GET_VERSION,
    MCU_INFO_CMD_PING,
)
from .frame import ProtoFrame
from .serial_session import H7SerialSession


class H7CommandClient:
    def __init__(
        self,
        session: H7SerialSession,
        event_callback: Callable[[ProtoFrame], None] | None = None,
    ) -> None:
        self.session = session
        self.seq = 0
        self.event_callback = event_callback

    def next_seq(self) -> int:
        self.seq = (self.seq + 1) & 0xFF

        if self.seq == 0:
            self.seq = 1

        return self.seq

    def request(
        self,
        cmd: int,
        payload: bytes = b"",
        timeout_s: float = 1.0,
        show_tx: bool = True,
        show_ignored_frames: bool = False,
    ) -> tuple[int, bytes, ProtoFrame | None]:
        seq = self.next_seq()
        tx = self.session.write_request(seq, cmd, payload)

        if show_tx:
            print(f"[PC] TX cmd=0x{cmd:02X}, seq={seq}: {tx.hex(' ').upper()}")

        frame = self.session.read_until(
            predicate=lambda f: f.is_response_for(seq, cmd),
            timeout_s=timeout_s,
            event_callback=self.event_callback,
            show_ignored_frames=show_ignored_frames,
        )

        return seq, tx, frame

    def ping(self, timeout_s: float = 1.0) -> tuple[int, bytes, ProtoFrame | None]:
        return self.request(MCU_INFO_CMD_PING, timeout_s=timeout_s)

    def get_version(self, timeout_s: float = 1.0) -> tuple[int, bytes, ProtoFrame | None]:
        return self.request(MCU_INFO_CMD_GET_VERSION, timeout_s=timeout_s)

    def get_status(self, timeout_s: float = 1.0) -> tuple[int, bytes, ProtoFrame | None]:
        return self.request(MCU_INFO_CMD_GET_STATUS, timeout_s=timeout_s)

    def get_time_info(self, timeout_s: float = 1.0) -> tuple[int, bytes, ProtoFrame | None]:
        return self.request(MCU_INFO_CMD_GET_TIME_INFO, timeout_s=timeout_s)

    def get_uart_stats(self, timeout_s: float = 1.0) -> tuple[int, bytes, ProtoFrame | None]:
        return self.request(MCU_INFO_CMD_GET_UART_STATS, timeout_s=timeout_s)

    def get_app_stats(self, timeout_s: float = 1.0) -> tuple[int, bytes, ProtoFrame | None]:
        return self.request(MCU_INFO_CMD_GET_APP_STATS, timeout_s=timeout_s)