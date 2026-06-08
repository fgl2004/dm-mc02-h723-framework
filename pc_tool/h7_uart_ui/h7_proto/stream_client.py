from __future__ import annotations

from dataclasses import dataclass

from .constants import (
    STREAM_MANAGER_DATA_CMD,
    TYPE_DATA,
    stream_channel_name,
)
from .frame import ProtoFrame


@dataclass
class StreamSample:
    channel_id: int
    seq: int
    timestamp_ms: int
    sample: bytes
    flags: int
    frame_seq: int

    @property
    def sample_len(self) -> int:
        return len(self.sample)

    def channel_name(self) -> str:
        return stream_channel_name(self.channel_id)

    def sample_hex(self) -> str:
        return self.sample.hex(" ").upper()

    def sample_ascii(self) -> str:
        return self.sample.decode("utf-8", errors="replace")


def decode_stream_sample(frame: ProtoFrame) -> StreamSample | None:
    """
    MCU StreamManager DATA payload format:

        byte 0      : channel_id
        byte 1..2   : stream sequence, little-endian uint16_t
        byte 3..6   : timestamp_ms, little-endian uint32_t
        byte 7      : sample_len
        byte 8..N   : sample bytes
    """
    if frame.frame_type != TYPE_DATA:
        return None

    if frame.cmd != STREAM_MANAGER_DATA_CMD:
        return None

    if len(frame.payload) < 8:
        return None

    payload = frame.payload

    channel_id = payload[0]
    seq = payload[1] | (payload[2] << 8)
    timestamp_ms = (
        payload[3]
        | (payload[4] << 8)
        | (payload[5] << 16)
        | (payload[6] << 24)
    )
    sample_len = payload[7]

    if len(payload) != 8 + sample_len:
        return None

    return StreamSample(
        channel_id=channel_id,
        seq=seq,
        timestamp_ms=timestamp_ms,
        sample=payload[8:],
        flags=frame.flags,
        frame_seq=frame.seq,
    )


class StreamClient:
    def __init__(self) -> None:
        self.rx_count = 0
        self.bad_count = 0
        self.last_by_channel: dict[int, StreamSample] = {}
        self.lost_by_channel: dict[int, int] = {}

    def handle_frame(self, frame: ProtoFrame) -> StreamSample | None:
        sample = decode_stream_sample(frame)

        if sample is None:
            self.bad_count += 1
            return None

        last = self.last_by_channel.get(sample.channel_id)
        if last is not None:
            expected = (last.seq + 1) & 0xFFFF
            if sample.seq != expected:
                lost = (sample.seq - expected) & 0xFFFF
                self.lost_by_channel[sample.channel_id] = (
                    self.lost_by_channel.get(sample.channel_id, 0) + lost
                )

        self.last_by_channel[sample.channel_id] = sample
        self.rx_count += 1

        return sample

    def summary(self) -> str:
        lost_total = sum(self.lost_by_channel.values())
        return f"stream_rx={self.rx_count},bad={self.bad_count},lost={lost_total}"


def stream_sample_summary(sample: StreamSample | None) -> str:
    if sample is None:
        return "invalid stream sample"

    return (
        f"channel=0x{sample.channel_id:02X}({sample.channel_name()}), "
        f"seq={sample.seq}, "
        f"timestamp={sample.timestamp_ms} ms, "
        f"sample_len={sample.sample_len}, "
        f"sample_hex=[{sample.sample_hex()}], "
        f"sample_ascii=[{sample.sample_ascii()}]"
    )
