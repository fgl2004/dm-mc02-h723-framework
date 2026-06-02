from __future__ import annotations

from dataclasses import dataclass
from typing import Optional


UARTSTAT_PREFIX = "@UARTSTAT"


@dataclass
class UartStat:
    host_time: float = 0.0

    t: int = 0
    rx_bytes: int = 0
    avail: int = 0
    free: int = 0
    rb_write: int = 0
    rb_read: int = 0
    overflow: int = 0
    rb_overflow: int = 0
    high: int = 0
    half: int = 0
    full: int = 0
    idle: int = 0
    err: int = 0

    @staticmethod
    def csv_header() -> str:
        return (
            "host_time,t,rx_bytes,avail,free,rb_write,rb_read,"
            "overflow,rb_overflow,high,half,full,idle,err\n"
        )

    def to_csv_row(self) -> str:
        return (
            f"{self.host_time:.3f},"
            f"{self.t},"
            f"{self.rx_bytes},"
            f"{self.avail},"
            f"{self.free},"
            f"{self.rb_write},"
            f"{self.rb_read},"
            f"{self.overflow},"
            f"{self.rb_overflow},"
            f"{self.high},"
            f"{self.half},"
            f"{self.full},"
            f"{self.idle},"
            f"{self.err}\n"
        )


def parse_uartstat_line(line: str, host_time: float = 0.0) -> Optional[UartStat]:
    line = line.strip()

    if not line.startswith(UARTSTAT_PREFIX):
        return None

    parts = line.split(",")
    data: dict[str, int] = {}

    for item in parts[1:]:
        if "=" not in item:
            continue

        key, value = item.split("=", 1)
        key = key.strip()
        value = value.strip()

        try:
            data[key] = int(value, 0)
        except ValueError:
            data[key] = 0

    return UartStat(
        host_time=host_time,
        t=data.get("t", 0),
        rx_bytes=data.get("rx_bytes", 0),
        avail=data.get("avail", 0),
        free=data.get("free", 0),
        rb_write=data.get("rb_write", 0),
        rb_read=data.get("rb_read", 0),
        overflow=data.get("overflow", 0),
        rb_overflow=data.get("rb_overflow", 0),
        high=data.get("high", 0),
        half=data.get("half", 0),
        full=data.get("full", 0),
        idle=data.get("idle", 0),
        err=data.get("err", 0),
    )