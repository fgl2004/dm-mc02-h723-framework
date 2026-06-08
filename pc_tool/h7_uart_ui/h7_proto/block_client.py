from __future__ import annotations

from dataclasses import dataclass

from .codec import build_frame
from .constants import (
    BLOCK_FLAG_MORE,
    BLOCK_FLAG_NEED_ACK,
    BLOCK_MANAGER_DATA_CMD,
    BLOCK_OP_ABORT,
    BLOCK_OP_ACK,
    BLOCK_OP_BEGIN,
    BLOCK_OP_CHUNK,
    BLOCK_OP_END,
    BLOCK_OP_NACK,
    TYPE_ACK,
    TYPE_DATA,
    TYPE_WINDOW_ACK,
    block_op_name,
)
from .frame import ProtoFrame


@dataclass
class BlockPacket:
    op: int
    handle: int
    offset: int
    data: bytes
    flags: int
    seq: int
    frame_type: int

    @property
    def data_len(self) -> int:
        return len(self.data)

    def op_name(self) -> str:
        return block_op_name(self.op)

    def data_hex(self) -> str:
        return self.data.hex(" ").upper()

    def data_ascii(self) -> str:
        return self.data.decode("utf-8", errors="replace")


def _u16_le(value: int) -> bytes:
    return bytes([value & 0xFF, (value >> 8) & 0xFF])


def _u32_le(value: int) -> bytes:
    return bytes(
        [
            value & 0xFF,
            (value >> 8) & 0xFF,
            (value >> 16) & 0xFF,
            (value >> 24) & 0xFF,
        ]
    )


def build_block_payload(op: int, handle: int, offset: int, data: bytes = b"") -> bytes:
    if len(data) > 0xFFFF:
        raise ValueError("block data too large")

    return (
        bytes([op & 0xFF, handle & 0xFF])
        + _u32_le(offset & 0xFFFFFFFF)
        + _u16_le(len(data))
        + bytes(data)
    )


def build_block_begin_frame(seq: int, handle: int, total_size: int, crc32: int = 0) -> bytes:
    metadata = _u32_le(total_size & 0xFFFFFFFF) + _u32_le(crc32 & 0xFFFFFFFF)
    payload = build_block_payload(BLOCK_OP_BEGIN, handle, 0, metadata)
    return build_frame(TYPE_DATA, BLOCK_FLAG_NEED_ACK, seq, BLOCK_MANAGER_DATA_CMD, payload)


def build_block_chunk_frame(
    seq: int,
    handle: int,
    offset: int,
    chunk: bytes,
    need_ack: bool = True,
    more: bool = True,
    retry: bool = False,
) -> bytes:
    flags = 0
    if need_ack:
        flags |= BLOCK_FLAG_NEED_ACK
    if more:
        flags |= BLOCK_FLAG_MORE
    if retry:
        # Keep local import optional so constants.py can grow without breaking older files.
        from .constants import BLOCK_FLAG_RETRY

        flags |= BLOCK_FLAG_RETRY

    payload = build_block_payload(BLOCK_OP_CHUNK, handle, offset, chunk)
    return build_frame(TYPE_DATA, flags, seq, BLOCK_MANAGER_DATA_CMD, payload)


def build_block_end_frame(seq: int, handle: int, crc32: int = 0) -> bytes:
    payload = build_block_payload(BLOCK_OP_END, handle, 0, _u32_le(crc32 & 0xFFFFFFFF))
    return build_frame(TYPE_DATA, BLOCK_FLAG_NEED_ACK, seq, BLOCK_MANAGER_DATA_CMD, payload)


def build_block_abort_frame(seq: int, handle: int, reason: int = 0) -> bytes:
    payload = build_block_payload(BLOCK_OP_ABORT, handle, 0, bytes([reason & 0xFF]))
    return build_frame(TYPE_DATA, BLOCK_FLAG_NEED_ACK, seq, BLOCK_MANAGER_DATA_CMD, payload)


def build_block_ack_frame(seq: int, handle: int, offset: int, accepted_len: int) -> bytes:
    payload = build_block_payload(BLOCK_OP_ACK, handle, offset, _u16_le(accepted_len & 0xFFFF))
    return build_frame(TYPE_ACK, 0, seq, BLOCK_MANAGER_DATA_CMD, payload)


def build_block_nack_frame(seq: int, handle: int, offset: int, error_code: int) -> bytes:
    payload = build_block_payload(BLOCK_OP_NACK, handle, offset, bytes([error_code & 0xFF]))
    return build_frame(TYPE_ACK, 0, seq, BLOCK_MANAGER_DATA_CMD, payload)


def decode_block_packet(frame: ProtoFrame) -> BlockPacket | None:
    if frame.frame_type not in (TYPE_DATA, TYPE_ACK, TYPE_WINDOW_ACK):
        return None

    if frame.cmd != BLOCK_MANAGER_DATA_CMD:
        return None

    if len(frame.payload) < 8:
        return None

    payload = frame.payload

    op = payload[0]
    handle = payload[1]
    offset = payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24)
    data_len = payload[6] | (payload[7] << 8)

    if len(payload) != 8 + data_len:
        return None

    return BlockPacket(
        op=op,
        handle=handle,
        offset=offset,
        data=payload[8:],
        flags=frame.flags,
        seq=frame.seq,
        frame_type=frame.frame_type,
    )


class BlockClient:
    def __init__(self) -> None:
        self.rx_count = 0
        self.bad_count = 0
        self.ack_count = 0
        self.nack_count = 0
        self.data_count = 0
        self.window_ack_count = 0
        self.last_packet: BlockPacket | None = None

    def handle_frame(self, frame: ProtoFrame) -> BlockPacket | None:
        packet = decode_block_packet(frame)

        if packet is None:
            self.bad_count += 1
            return None

        self.rx_count += 1
        self.last_packet = packet

        if frame.frame_type == TYPE_WINDOW_ACK:
            self.window_ack_count += 1
        elif packet.op == BLOCK_OP_ACK:
            self.ack_count += 1
        elif packet.op == BLOCK_OP_NACK:
            self.nack_count += 1
        else:
            self.data_count += 1

        return packet

    def summary(self) -> str:
        return (
            f"block_rx={self.rx_count},data={self.data_count},ack={self.ack_count},"
            f"nack={self.nack_count},window_ack={self.window_ack_count},bad={self.bad_count}"
        )


def block_packet_summary(packet: BlockPacket | None) -> str:
    if packet is None:
        return "invalid block packet"

    return (
        f"op=0x{packet.op:02X}({packet.op_name()}), "
        f"handle=0x{packet.handle:02X}, "
        f"offset={packet.offset}, "
        f"len={packet.data_len}, "
        f"flags=0x{packet.flags:02X}, "
        f"seq={packet.seq}, "
        f"data_hex=[{packet.data_hex()}], "
        f"data_ascii=[{packet.data_ascii()}]"
    )
