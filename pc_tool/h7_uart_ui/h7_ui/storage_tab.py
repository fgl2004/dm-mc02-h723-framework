from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt, QTimer
from PySide6.QtWidgets import (
    QFileDialog,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMessageBox,
    QPlainTextEdit,
    QProgressBar,
    QPushButton,
    QSpinBox,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from h7_proto.block_client import BlockPacket
from h7_proto.constants import (
    STORAGE_CMD_ERASE_PARTITION,
    STORAGE_CMD_GET_INFO,
    STORAGE_CMD_GET_PARTITION_COUNT,
    STORAGE_CMD_GET_PARTITION_INFO,
    STORAGE_CMD_GET_STATS,
    STORAGE_CMD_READ_PARTITION_BEGIN,
    STORAGE_CMD_WRITE_PARTITION_BEGIN,
    storage_cmd_name,
)
from h7_proto.frame import ProtoFrame
from h7_proto.storage_client import (
    StorageClient,
    StoragePartitionInfo,
    decode_partition_count_response,
    decode_partition_info_response,
    decode_stats_response,
    decode_storage_info_response,
    storage_response_status_text,
)


class StorageTab(QWidget):
    def __init__(self, serial_worker, parent=None) -> None:
        super().__init__(parent)
        self.serial_worker = serial_worker
        self.client = StorageClient(serial_worker)
        self.partitions: dict[int, StoragePartitionInfo] = {}
        self.expected_partition_count = 0
        self.download_save_path: Path | None = None

        self._build_ui()

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)

        top = QHBoxLayout()
        top.addWidget(self._build_info_group(), stretch=1)
        top.addWidget(self._build_ops_group(), stretch=1)
        layout.addLayout(top)

        layout.addWidget(self._build_partition_group(), stretch=3)
        layout.addWidget(self._build_log_group(), stretch=2)

    def _build_info_group(self) -> QGroupBox:
        group = QGroupBox("Storage Info")
        layout = QGridLayout(group)

        self.refresh_info_btn = QPushButton("Get Flash Info")
        self.refresh_partitions_btn = QPushButton("Refresh Partitions")
        self.storage_stats_btn = QPushButton("Get Stats")
        self.abort_btn = QPushButton("Abort Transfer")

        self.info_labels: dict[str, QLabel] = {}
        names = ["capacity", "read", "program", "erase", "partitions", "last_status"]
        for idx, name in enumerate(names):
            layout.addWidget(QLabel(name + ":"), idx, 0)
            label = QLabel("-")
            label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
            layout.addWidget(label, idx, 1)
            self.info_labels[name] = label

        layout.addWidget(self.refresh_info_btn, 0, 2)
        layout.addWidget(self.refresh_partitions_btn, 1, 2)
        layout.addWidget(self.storage_stats_btn, 2, 2)
        layout.addWidget(self.abort_btn, 3, 2)

        self.refresh_info_btn.clicked.connect(self.request_info)
        self.refresh_partitions_btn.clicked.connect(self.refresh_partitions)
        self.storage_stats_btn.clicked.connect(self.request_stats)
        self.abort_btn.clicked.connect(self.abort_transfer)

        return group

    def _build_ops_group(self) -> QGroupBox:
        group = QGroupBox("Raw Partition Test")
        layout = QGridLayout(group)

        self.partition_id_spin = QSpinBox()
        self.partition_id_spin.setRange(0, 255)
        self.partition_id_spin.setValue(6)

        self.offset_spin = QSpinBox()
        self.offset_spin.setRange(0, 0x7FFFFF)
        self.offset_spin.setValue(0)
        self.offset_spin.setPrefix("0x")
        self.offset_spin.setDisplayIntegerBase(16)

        self.length_spin = QSpinBox()
        self.length_spin.setRange(1, 8 * 1024 * 1024)
        self.length_spin.setValue(1024)

        self.erase_btn = QPushButton("Erase Partition")
        self.upload_btn = QPushButton("Upload File To Partition")
        self.download_btn = QPushButton("Download Partition Range")
        self.progress = QProgressBar()
        self.progress.setRange(0, 100)

        layout.addWidget(QLabel("partition id:"), 0, 0)
        layout.addWidget(self.partition_id_spin, 0, 1)
        layout.addWidget(QLabel("offset:"), 1, 0)
        layout.addWidget(self.offset_spin, 1, 1)
        layout.addWidget(QLabel("length:"), 2, 0)
        layout.addWidget(self.length_spin, 2, 1)
        layout.addWidget(self.erase_btn, 0, 2)
        layout.addWidget(self.upload_btn, 1, 2)
        layout.addWidget(self.download_btn, 2, 2)
        layout.addWidget(self.progress, 3, 0, 1, 3)

        self.erase_btn.clicked.connect(self.erase_partition)
        self.upload_btn.clicked.connect(self.upload_file)
        self.download_btn.clicked.connect(self.download_range)

        return group

    def _build_partition_group(self) -> QGroupBox:
        group = QGroupBox("Partition Table")
        layout = QVBoxLayout(group)

        self.partition_table = QTableWidget(0, 9)
        self.partition_table.setHorizontalHeaderLabels(
            ["id", "name", "start", "size", "erase", "flags", "R", "W", "E"]
        )
        self.partition_table.cellClicked.connect(self._on_partition_cell_clicked)
        layout.addWidget(self.partition_table)
        return group

    def _build_log_group(self) -> QGroupBox:
        group = QGroupBox("Storage Log")
        layout = QVBoxLayout(group)
        self.log_text = QPlainTextEdit()
        self.log_text.setReadOnly(True)
        layout.addWidget(self.log_text)
        return group

    def request_info(self) -> None:
        if not self._is_open():
            return
        self.client.request_get_info()
        self._log("[TX] STORAGE_GET_INFO")

    def refresh_partitions(self) -> None:
        if not self._is_open():
            return
        self.partitions.clear()
        self.partition_table.setRowCount(0)
        self.client.request_partition_count()
        self._log("[TX] STORAGE_GET_PARTITION_COUNT")

    def request_stats(self) -> None:
        if not self._is_open():
            return
        self.client.request_stats()
        self._log("[TX] STORAGE_GET_STATS")

    def abort_transfer(self) -> None:
        if not self._is_open():
            return
        self.client.request_abort_transfer()
        self._log("[TX] STORAGE_ABORT_TRANSFER")

    def erase_partition(self) -> None:
        if not self._is_open():
            return
        pid = self.partition_id_spin.value()
        ret = QMessageBox.question(
            self,
            "Confirm erase",
            f"Erase partition {pid}? This will destroy data in that partition.",
        )
        if ret != QMessageBox.Yes:
            return
        self.client.request_erase_partition(pid)
        self._log(f"[TX] STORAGE_ERASE_PARTITION id={pid}")

    def upload_file(self) -> None:
        if not self._is_open():
            return
        filename, _ = QFileDialog.getOpenFileName(self, "Select file to upload")
        if not filename:
            return
        data = Path(filename).read_bytes()
        pid = self.partition_id_spin.value()
        offset = self.offset_spin.value()
        seq, handle, crc32 = self.client.request_write_partition_begin(pid, offset, data)
        self._log(
            f"[TX] STORAGE_WRITE_PARTITION_BEGIN seq={seq}, id={pid}, offset=0x{offset:X}, "
            f"size={len(data)}, handle=0x{handle:02X}, crc32=0x{crc32:08X}"
        )
        QTimer.singleShot(80, lambda: self._send_upload_blocks_later(data, handle, crc32))

    def _send_upload_blocks_later(self, data: bytes, handle: int, crc32: int) -> None:
        try:
            self.client.send_upload_blocks(data, handle, crc32)
            self.progress.setValue(100)
            self._log(f"[TX] upload blocks queued: size={len(data)}, handle=0x{handle:02X}")
        except Exception as exc:
            self._log(f"[ERROR] upload blocks failed: {exc}")

    def download_range(self) -> None:
        if not self._is_open():
            return
        filename, _ = QFileDialog.getSaveFileName(self, "Save downloaded binary", "mcu_flash_range.bin")
        if not filename:
            return
        self.download_save_path = Path(filename)
        pid = self.partition_id_spin.value()
        offset = self.offset_spin.value()
        length = self.length_spin.value()
        seq, handle = self.client.request_read_partition_begin(pid, offset, length)
        self.progress.setValue(0)
        self._log(
            f"[TX] STORAGE_READ_PARTITION_BEGIN seq={seq}, id={pid}, offset=0x{offset:X}, "
            f"len={length}, handle=0x{handle:02X}"
        )

    def handle_response(self, frame: ProtoFrame) -> None:
        if frame.cmd not in {
            STORAGE_CMD_GET_INFO,
            STORAGE_CMD_GET_PARTITION_COUNT,
            STORAGE_CMD_GET_PARTITION_INFO,
            STORAGE_CMD_ERASE_PARTITION,
            STORAGE_CMD_WRITE_PARTITION_BEGIN,
            STORAGE_CMD_READ_PARTITION_BEGIN,
            STORAGE_CMD_GET_STATS,
        }:
            return

        status = storage_response_status_text(frame)
        self.info_labels["last_status"].setText(status)
        self._log(f"[RESP] {storage_cmd_name(frame.cmd)} status={status}, len={len(frame.payload)}")

        if frame.cmd == STORAGE_CMD_GET_INFO:
            info = decode_storage_info_response(frame)
            if info is not None:
                self.info_labels["capacity"].setText(f"{info.capacity} B")
                self.info_labels["read"].setText(str(info.read_size))
                self.info_labels["program"].setText(str(info.program_size))
                self.info_labels["erase"].setText(str(info.erase_size))
                self.info_labels["partitions"].setText(str(info.partition_count))

        elif frame.cmd == STORAGE_CMD_GET_PARTITION_COUNT:
            count = decode_partition_count_response(frame)
            if count is not None:
                self.expected_partition_count = count
                self.info_labels["partitions"].setText(str(count))
                for pid in range(count):
                    self.client.request_partition_info(pid)

        elif frame.cmd == STORAGE_CMD_GET_PARTITION_INFO:
            part = decode_partition_info_response(frame)
            if part is not None:
                self.partitions[part.partition_id] = part
                self._update_partition_table()

        elif frame.cmd == STORAGE_CMD_GET_STATS:
            stats = decode_stats_response(frame)
            if stats is not None:
                self._log(
                    "[STATS] "
                    f"read={stats.read_count}/{stats.read_bytes}B, "
                    f"write={stats.write_count}/{stats.write_bytes}B, "
                    f"erase={stats.erase_count}/{stats.erase_bytes}B, "
                    f"bts_written={stats.transfer_bytes_written}, bts_read={stats.transfer_bytes_read}, "
                    f"state={stats.transfer_state}, last_error={stats.transfer_last_error}"
                )

    def handle_block_packet(self, packet: BlockPacket) -> None:
        msg = self.client.handle_block_packet(packet)
        if msg is None:
            return
        self._log(f"[BLOCK] {msg}")

        pending = self.client.pending_download
        if pending is not None and pending.get("length"):
            percent = min(100, int(len(self.client.last_download_data) * 100 / pending["length"]))
            self.progress.setValue(percent)

        if pending is not None and pending.get("complete") and self.download_save_path is not None:
            self.download_save_path.write_bytes(bytes(self.client.last_download_data))
            self._log(f"[FILE] saved {len(self.client.last_download_data)} bytes to {self.download_save_path}")
            self.progress.setValue(100)
            self.download_save_path = None
            self.client.pending_download = None

    def _update_partition_table(self) -> None:
        items = [self.partitions[k] for k in sorted(self.partitions.keys())]
        self.partition_table.setRowCount(len(items))
        for row, part in enumerate(items):
            values = [
                str(part.partition_id),
                part.name,
                f"0x{part.start_addr:06X}",
                f"{part.size_bytes // 1024} KB",
                str(part.erase_size),
                f"0x{part.flags:08X}",
                "Y" if part.readable else "-",
                "Y" if part.writable else "-",
                "Y" if part.erasable else "-",
            ]
            for col, value in enumerate(values):
                self.partition_table.setItem(row, col, QTableWidgetItem(value))
        self.partition_table.resizeColumnsToContents()

    def _on_partition_cell_clicked(self, row: int, col: int) -> None:
        del col
        item = self.partition_table.item(row, 0)
        if item is not None:
            self.partition_id_spin.setValue(int(item.text()))

    def _is_open(self) -> bool:
        if not self.serial_worker.is_open():
            self._log("[ERROR] serial port is not open")
            return False
        return True

    def _log(self, text: str) -> None:
        print(text)
        self.log_text.appendPlainText(text)