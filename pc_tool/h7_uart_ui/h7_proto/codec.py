from __future__ import annotations

from .constants import MAX_PAYLOAD, SOF1, SOF2, VER
from .crc16 import crc16_ccitt_false
from .frame import ProtoFrame


def build_frame(
    frame_type: int,
    flags: int,
    seq: int,
    cmd: int,
    payload: bytes = b"",
) -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload too large: {len(payload)} > {MAX_PAYLOAD}")

    length = len(payload)

    header = bytes(
        [
            VER,
            frame_type & 0xFF,
            flags & 0xFF,
            seq & 0xFF,
            cmd & 0xFF,
            length & 0xFF,
            (length >> 8) & 0xFF,
        ]
    )

    crc = crc16_ccitt_false(header + payload)

    return bytes([SOF1, SOF2]) + header + payload + bytes(
        [
            crc & 0xFF,
            (crc >> 8) & 0xFF,
        ]
    )


def build_frame_with_raw_len(
    frame_type: int,
    flags: int,
    seq: int,
    cmd: int,
    raw_len: int,
    payload: bytes = b"",
) -> bytes:
    header = bytes(
        [
            VER,
            frame_type & 0xFF,
            flags & 0xFF,
            seq & 0xFF,
            cmd & 0xFF,
            raw_len & 0xFF,
            (raw_len >> 8) & 0xFF,
        ]
    )

    crc = crc16_ccitt_false(header + payload)

    return bytes([SOF1, SOF2]) + header + payload + bytes(
        [
            crc & 0xFF,
            (crc >> 8) & 0xFF,
        ]
    )


def corrupt_last_crc_byte(frame: bytes) -> bytes:
    if len(frame) < 2:
        return frame

    data = bytearray(frame)
    data[-1] ^= 0x55
    return bytes(data)


class ProtocolStreamParser:
    def __init__(self) -> None:
        self._buf = bytearray()
        self.input_bytes = 0
        self.frame_count = 0
        self.crc_error_count = 0
        self.len_error_count = 0
        self.version_error_count = 0
        self.drop_bytes = 0

    def reset(self) -> None:
        self._buf.clear()

    def feed(self, data: bytes) -> list[ProtoFrame]:
        if not data:
            return []

        self._buf.extend(data)
        self.input_bytes += len(data)

        frames: list[ProtoFrame] = []

        while True:
            frame = self._try_parse_one()
            if frame is None:
                break

            frames.append(frame)
            self.frame_count += 1

        return frames

    def _try_parse_one(self) -> ProtoFrame | None:
        while len(self._buf) >= 2:
            if self._buf[0] == SOF1 and self._buf[1] == SOF2:
                break

            del self._buf[0]
            self.drop_bytes += 1

        if len(self._buf) < 11:
            return None

        ver = self._buf[2]
        frame_type = self._buf[3]
        flags = self._buf[4]
        seq = self._buf[5]
        cmd = self._buf[6]
        length = self._buf[7] | (self._buf[8] << 8)

        if ver != VER:
            del self._buf[0]
            self.version_error_count += 1
            return self._try_parse_one()

        if length > MAX_PAYLOAD:
            del self._buf[0]
            self.len_error_count += 1
            return self._try_parse_one()

        total_len = 2 + 7 + length + 2

        if len(self._buf) < total_len:
            return None

        frame_bytes = bytes(self._buf[:total_len])
        del self._buf[:total_len]

        recv_crc = frame_bytes[-2] | (frame_bytes[-1] << 8)
        calc_crc = crc16_ccitt_false(frame_bytes[2:-2])

        if recv_crc != calc_crc:
            self.crc_error_count += 1
            return self._try_parse_one()

        payload = frame_bytes[9:-2]

        return ProtoFrame(
            frame_type=frame_type,
            flags=flags,
            seq=seq,
            cmd=cmd,
            payload=payload,
        )