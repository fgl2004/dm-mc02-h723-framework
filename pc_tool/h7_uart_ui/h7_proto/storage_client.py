from __future__ import annotations

import binascii
from dataclasses import dataclass

from .block_client import (
    BlockPacket,
    build_block_begin_frame,
    build_block_chunk_frame,
    build_block_end_frame,
)
from .constants import (
    BLOCK_OP_BEGIN,
    BLOCK_OP_CHUNK,
    BLOCK_OP_END,
    STORAGE_CMD_ABORT_TRANSFER,
    STORAGE_CMD_ERASE_PARTITION,
    STORAGE_CMD_GET_INFO,
    STORAGE_CMD_GET_PARTITION_COUNT,
    STORAGE_CMD_GET_PARTITION_INFO,
    STORAGE_CMD_GET_STATS,
    STORAGE_CMD_READ_PARTITION_BEGIN,
    STORAGE_CMD_WRITE_PARTITION_BEGIN,
    STORAGE_STATUS_OK,
    storage_status_name,
)
from .frame import ProtoFrame


def _u32_le(value: int) -> bytes:
    return bytes(
        [
            value & 0xFF,
            (value >> 8) & 0xFF,
            (value >> 16) & 0xFF,
            (value >> 24) & 0xFF,
        ]
    )


def _read_u32(payload: bytes, offset: int) -> int:
    return (
        payload[offset]
        | (payload[offset + 1] << 8)
        | (payload[offset + 2] << 16)
        | (payload[offset + 3] << 24)
    )


@dataclass
class StorageInfo:
    capacity: int
    read_size: int
    program_size: int
    erase_size: int
    partition_count: int


@dataclass
class StoragePartitionInfo:
    partition_id: int
    access: int
    name: str
    start_addr: int
    size_bytes: int
    erase_size: int
    flags: int
    valid: bool

    @property
    def readable(self) -> bool:
        return bool(self.access & 0x01)

    @property
    def writable(self) -> bool:
        return bool(self.access & 0x02)

    @property
    def erasable(self) -> bool:
        return bool(self.access & 0x04)


@dataclass
class StorageStats:
    read_count: int
    write_count: int
    erase_count: int
    read_bytes: int
    write_bytes: int
    erase_bytes: int
    transfer_bytes_written: int
    transfer_bytes_read: int
    transfer_state: int
    transfer_last_error: int


class StorageClient:
    def __init__(self, serial_worker) -> None:
        self.serial_worker = serial_worker
        self.default_handle = 0x31
        self.upload_chunk_size = 112
        self.pending_download: dict | None = None
        self.last_download_data = bytearray()
        self.last_download_crc32 = 0

    def request_get_info(self) -> tuple[int, bytes]:
        return self.serial_worker.send_request(STORAGE_CMD_GET_INFO)

    def request_partition_count(self) -> tuple[int, bytes]:
        return self.serial_worker.send_request(STORAGE_CMD_GET_PARTITION_COUNT)

    def request_partition_info(self, partition_id: int) -> tuple[int, bytes]:
        return self.serial_worker.send_request(
            STORAGE_CMD_GET_PARTITION_INFO,
            bytes([partition_id & 0xFF]),
        )

    def request_erase_partition(self, partition_id: int) -> tuple[int, bytes]:
        return self.serial_worker.send_request(
            STORAGE_CMD_ERASE_PARTITION,
            bytes([partition_id & 0xFF]),
        )

    def request_stats(self) -> tuple[int, bytes]:
        return self.serial_worker.send_request(STORAGE_CMD_GET_STATS)

    def request_abort_transfer(self) -> tuple[int, bytes]:
        return self.serial_worker.send_request(STORAGE_CMD_ABORT_TRANSFER)

    def request_write_partition_begin(
        self,
        partition_id: int,
        offset: int,
        data: bytes,
        handle: int | None = None,
    ) -> tuple[int, int, int]:
        handle = self.default_handle if handle is None else (handle & 0xFF)
        crc32 = binascii.crc32(data) & 0xFFFFFFFF
        payload = (
            bytes([partition_id & 0xFF])
            + _u32_le(offset)
            + _u32_le(len(data))
            + _u32_le(crc32)
            + bytes([handle])
        )
        seq, _ = self.serial_worker.send_request(STORAGE_CMD_WRITE_PARTITION_BEGIN, payload)
        return seq, handle, crc32

    def send_upload_blocks(self, data: bytes, handle: int, crc32: int) -> None:
        seq = self.serial_worker._next_seq()
        self.serial_worker.enqueue_tx(build_block_begin_frame(seq, handle, len(data), crc32))

        offset = 0
        while offset < len(data):
            chunk = data[offset : offset + self.upload_chunk_size]
            offset_next = offset + len(chunk)
            more = offset_next < len(data)
            seq = self.serial_worker._next_seq()
            self.serial_worker.enqueue_tx(
                build_block_chunk_frame(
                    seq=seq,
                    handle=handle,
                    offset=offset,
                    chunk=chunk,
                    need_ack=True,
                    more=more,
                )
            )
            offset = offset_next

        seq = self.serial_worker._next_seq()
        self.serial_worker.enqueue_tx(build_block_end_frame(seq, handle, crc32))

    def request_read_partition_begin(
        self,
        partition_id: int,
        offset: int,
        length: int,
        handle: int | None = None,
    ) -> tuple[int, int]:
        handle = self.default_handle if handle is None else (handle & 0xFF)
        payload = bytes([partition_id & 0xFF]) + _u32_le(offset) + _u32_le(length) + bytes([handle])
        self.pending_download = {
            "partition_id": partition_id,
            "offset": offset,
            "length": length,
            "handle": handle,
            "complete": False,
        }
        self.last_download_data = bytearray()
        self.last_download_crc32 = 0
        seq, _ = self.serial_worker.send_request(STORAGE_CMD_READ_PARTITION_BEGIN, payload)
        return seq, handle

    def handle_block_packet(self, packet: BlockPacket) -> str | None:
        if self.pending_download is None:
            return None

        if packet.handle != self.pending_download["handle"]:
            return None

        if packet.op == BLOCK_OP_BEGIN:
            self.last_download_data = bytearray()
            self.last_download_crc32 = 0
            return f"download begin: total={packet.data.hex(' ').upper()}"

        if packet.op == BLOCK_OP_CHUNK:
            end = packet.offset + packet.data_len
            if len(self.last_download_data) < end:
                self.last_download_data.extend(b"\x00" * (end - len(self.last_download_data)))
            self.last_download_data[packet.offset:end] = packet.data
            self.last_download_crc32 = binascii.crc32(packet.data, self.last_download_crc32) & 0xFFFFFFFF
            return f"download chunk: offset={packet.offset}, len={packet.data_len}, total={len(self.last_download_data)}"

        if packet.op == BLOCK_OP_END:
            self.pending_download["complete"] = True
            return f"download end: size={len(self.last_download_data)}, crc32=0x{self.last_download_crc32:08X}"

        return None


def decode_storage_info_response(frame: ProtoFrame) -> StorageInfo | None:
    p = frame.payload
    if len(p) < 21 or p[0] != STORAGE_STATUS_OK:
        return None
    return StorageInfo(
        capacity=_read_u32(p, 1),
        read_size=_read_u32(p, 5),
        program_size=_read_u32(p, 9),
        erase_size=_read_u32(p, 13),
        partition_count=_read_u32(p, 17),
    )


def decode_partition_count_response(frame: ProtoFrame) -> int | None:
    p = frame.payload
    if len(p) < 5 or p[0] != STORAGE_STATUS_OK:
        return None
    return _read_u32(p, 1)


def decode_partition_info_response(frame: ProtoFrame) -> StoragePartitionInfo | None:
    p = frame.payload
    if len(p) < 24 or p[0] != STORAGE_STATUS_OK:
        return None
    name_len = p[3]
    if len(p) < 24 + name_len:
        return None
    return StoragePartitionInfo(
        partition_id=p[1],
        access=p[2],
        name=p[24 : 24 + name_len].decode("utf-8", errors="replace"),
        start_addr=_read_u32(p, 4),
        size_bytes=_read_u32(p, 8),
        erase_size=_read_u32(p, 12),
        flags=_read_u32(p, 16),
        valid=bool(_read_u32(p, 20)),
    )


def decode_stats_response(frame: ProtoFrame) -> StorageStats | None:
    p = frame.payload
    if len(p) < 41 or p[0] != STORAGE_STATUS_OK:
        return None
    return StorageStats(
        read_count=_read_u32(p, 1),
        write_count=_read_u32(p, 5),
        erase_count=_read_u32(p, 9),
        read_bytes=_read_u32(p, 13),
        write_bytes=_read_u32(p, 17),
        erase_bytes=_read_u32(p, 21),
        transfer_bytes_written=_read_u32(p, 25),
        transfer_bytes_read=_read_u32(p, 29),
        transfer_state=_read_u32(p, 33),
        transfer_last_error=_read_u32(p, 37),
    )


def storage_response_status_text(frame: ProtoFrame) -> str:
    if not frame.payload:
        return "NO_STATUS"
    return storage_status_name(frame.payload[0])
