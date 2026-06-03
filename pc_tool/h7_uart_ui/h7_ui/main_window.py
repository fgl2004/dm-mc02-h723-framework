from __future__ import annotations

import time
from collections import deque

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QTextCursor
from PySide6.QtWidgets import (
    QMainWindow,
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QGridLayout,
    QLabel,
    QPushButton,
    QComboBox,
    QSpinBox,
    QDoubleSpinBox,
    QTextEdit,
    QGroupBox,
    QProgressBar,
    QMessageBox,
    QCheckBox,
)

import pyqtgraph as pg

from h7_proto.constants import (
    MCU_INFO_CMD_GET_APP_STATS,
    MCU_INFO_CMD_GET_RESET_INFO,
    MCU_INFO_CMD_GET_STATUS,
    MCU_INFO_CMD_GET_TIME_INFO,
    MCU_INFO_CMD_GET_UART_STATS,
    MCU_INFO_CMD_GET_VERSION,
    MCU_INFO_CMD_PING,
    event_name,
    error_name,
)
from h7_proto.event import decode_mcu_info_event, event_summary
from h7_proto.frame import ProtoFrame

from .patterns import AVAILABLE_PATTERNS, make_pattern
from .serial_worker import SerialWorker, list_serial_ports
from .telemetry import UartStat
from .ring_buffer_widget import RingBufferWidget


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()

        print("[DEBUG] MainWindow.__init__()")

        self.setWindowTitle("H7 UART DMA RX + Protocol Monitor")
        self.resize(1500, 940)

        self.serial_worker = SerialWorker()

        self.stats_history_len = 300
        self.time_data = deque(maxlen=self.stats_history_len)
        self.avail_data = deque(maxlen=self.stats_history_len)
        self.high_data = deque(maxlen=self.stats_history_len)
        self.rx_rate_data = deque(maxlen=self.stats_history_len)

        self.last_rx_bytes: int | None = None
        self.last_host_time: float | None = None

        self.csv_file = None

        # Stress TX rate control
        self.stress_timer = QTimer(self)
        self.stress_timer.timeout.connect(self._stress_enqueue_next_chunk)

        self.stress_running = False
        self.stress_data = b""
        self.stress_total = 0
        self.stress_enqueued = 0
        self.stress_tx_written = 0
        self.stress_start_time = 0.0
        self.stress_last_queue_size = 0
        self.stress_chunk = 64

        # Protocol monitor
        self.protocol_frame_count = 0
        self.protocol_resp_count = 0
        self.protocol_nack_count = 0
        self.protocol_event_count = 0
        self.protocol_tx_req_count = 0
        self.protocol_last_seq = 0
        self.protocol_auto_poll_index = 0

        self.protocol_poll_timer = QTimer(self)
        self.protocol_poll_timer.timeout.connect(self._protocol_auto_poll_once)

        self._build_ui()
        self._connect_signals()
        self._refresh_ports()
        self._update_target_rate_label()

        self._append_log("[UI] MainWindow initialized")

    def closeEvent(self, event) -> None:
        try:
            self._append_log("[UI] Closing window")

            if self.csv_file is not None:
                self.csv_file.close()
                self.csv_file = None

            if self.protocol_poll_timer.isActive():
                self.protocol_poll_timer.stop()

            self._stop_stress_silent()
            self.serial_worker.disconnect_port()

        finally:
            event.accept()

    # -------------------------------------------------------------------------
    # UI
    # -------------------------------------------------------------------------

    def _build_ui(self) -> None:
        root = QWidget()
        layout = QVBoxLayout(root)

        layout.addWidget(self._build_connection_group())
        layout.addWidget(self._build_metrics_group())

        middle_layout = QHBoxLayout()
        middle_layout.addWidget(self._build_visual_group(), stretch=3)
        middle_layout.addWidget(self._build_protocol_group(), stretch=2)

        layout.addLayout(middle_layout, stretch=4)

        layout.addWidget(self._build_stress_group())
        layout.addWidget(self._build_log_group(), stretch=2)

        self.setCentralWidget(root)

    def _build_connection_group(self) -> QGroupBox:
        group = QGroupBox("Connection")
        layout = QHBoxLayout(group)

        self.port_combo = QComboBox()

        self.baud_combo = QComboBox()
        self.baud_combo.addItems(["115200", "230400", "460800", "921600"])
        self.baud_combo.setCurrentText("115200")

        self.refresh_btn = QPushButton("Refresh")
        self.connect_btn = QPushButton("Connect")
        self.disconnect_btn = QPushButton("Disconnect")
        self.disconnect_btn.setEnabled(False)

        self.status_label = QLabel("Disconnected")

        layout.addWidget(QLabel("Port:"))
        layout.addWidget(self.port_combo)
        layout.addWidget(QLabel("Baud:"))
        layout.addWidget(self.baud_combo)
        layout.addWidget(self.refresh_btn)
        layout.addWidget(self.connect_btn)
        layout.addWidget(self.disconnect_btn)
        layout.addStretch(1)
        layout.addWidget(self.status_label)

        return group

    def _build_metrics_group(self) -> QGroupBox:
        group = QGroupBox("UART RX Metrics")
        layout = QGridLayout(group)

        self.metric_labels: dict[str, QLabel] = {}

        names = [
            "t",
            "rx_bytes",
            "rx_rate",
            "avail",
            "free",
            "rb_write",
            "rb_read",
            "overflow",
            "rb_overflow",
            "high",
            "half",
            "full",
            "idle",
            "err",
            "tx_written",
        ]

        for idx, name in enumerate(names):
            row = idx // 8
            col = (idx % 8) * 2

            layout.addWidget(QLabel(name + ":"), row, col)

            value_label = QLabel("0")
            value_label.setMinimumWidth(80)
            value_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)

            layout.addWidget(value_label, row, col + 1)

            self.metric_labels[name] = value_label

        return group

    def _build_visual_group(self) -> QGroupBox:
        group = QGroupBox("Live Visualization")
        layout = QHBoxLayout(group)

        self.ring_widget = RingBufferWidget()
        layout.addWidget(self.ring_widget, stretch=1)

        right_layout = QVBoxLayout()

        pg.setConfigOptions(antialias=True)

        self.plot_rate = pg.PlotWidget(title="RX byte rate")
        self.plot_rate.showGrid(x=True, y=True)
        self.curve_rx_rate = self.plot_rate.plot(name="rx_rate")

        self.plot_buffer = pg.PlotWidget(title="RingBuffer available / high watermark")
        self.plot_buffer.showGrid(x=True, y=True)
        self.curve_avail = self.plot_buffer.plot(name="avail")
        self.curve_high = self.plot_buffer.plot(name="high")

        right_layout.addWidget(self.plot_rate)
        right_layout.addWidget(self.plot_buffer)

        layout.addLayout(right_layout, stretch=3)

        return group

    def _build_protocol_group(self) -> QGroupBox:
        group = QGroupBox("Protocol Monitor")
        layout = QVBoxLayout(group)

        stats_group = QGroupBox("Protocol Stats")
        stats_layout = QGridLayout(stats_group)

        self.protocol_labels: dict[str, QLabel] = {}
        names = [
            "frames",
            "resp",
            "nack",
            "event",
            "tx_req",
            "last_seq",
        ]

        for idx, name in enumerate(names):
            row = idx // 3
            col = (idx % 3) * 2

            stats_layout.addWidget(QLabel(name + ":"), row, col)

            value_label = QLabel("0")
            value_label.setMinimumWidth(70)
            value_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)

            stats_layout.addWidget(value_label, row, col + 1)
            self.protocol_labels[name] = value_label

        layout.addWidget(stats_group)

        cmd_group = QGroupBox("Command")
        cmd_layout = QGridLayout(cmd_group)

        self.proto_ping_btn = QPushButton("PING")
        self.proto_version_btn = QPushButton("VERSION")
        self.proto_status_btn = QPushButton("STATUS")
        self.proto_time_btn = QPushButton("TIME")
        self.proto_reset_btn = QPushButton("RESET_INFO")
        self.proto_uart_btn = QPushButton("UART_STATS")
        self.proto_app_btn = QPushButton("APP_STATS")

        self.proto_auto_poll_check = QCheckBox("Auto Poll")
        self.proto_poll_interval_spin = QSpinBox()
        self.proto_poll_interval_spin.setRange(100, 10000)
        self.proto_poll_interval_spin.setValue(1000)
        self.proto_poll_interval_spin.setSuffix(" ms")

        cmd_layout.addWidget(self.proto_ping_btn, 0, 0)
        cmd_layout.addWidget(self.proto_version_btn, 0, 1)
        cmd_layout.addWidget(self.proto_status_btn, 0, 2)

        cmd_layout.addWidget(self.proto_time_btn, 1, 0)
        cmd_layout.addWidget(self.proto_reset_btn, 1, 1)
        cmd_layout.addWidget(self.proto_uart_btn, 1, 2)

        cmd_layout.addWidget(self.proto_app_btn, 2, 0)

        cmd_layout.addWidget(self.proto_auto_poll_check, 3, 0)
        cmd_layout.addWidget(QLabel("Interval:"), 3, 1)
        cmd_layout.addWidget(self.proto_poll_interval_spin, 3, 2)

        layout.addWidget(cmd_group)

        self.protocol_log_text = QTextEdit()
        self.protocol_log_text.setReadOnly(True)
        self.protocol_log_text.setMinimumHeight(170)

        self.event_log_text = QTextEdit()
        self.event_log_text.setReadOnly(True)
        self.event_log_text.setMinimumHeight(170)

        layout.addWidget(QLabel("RESP / NACK / TX Log"))
        layout.addWidget(self.protocol_log_text, stretch=1)

        layout.addWidget(QLabel("EVENT Log"))
        layout.addWidget(self.event_log_text, stretch=1)

        return group

    def _build_stress_group(self) -> QGroupBox:
        group = QGroupBox("Stress Test / TX Rate Control")
        layout = QHBoxLayout(group)

        self.pattern_combo = QComboBox()
        self.pattern_combo.addItems(AVAILABLE_PATTERNS)

        self.size_spin = QSpinBox()
        self.size_spin.setRange(0, 100_000_000)
        self.size_spin.setValue(1024)

        self.chunk_spin = QSpinBox()
        self.chunk_spin.setRange(1, 65535)
        self.chunk_spin.setValue(64)

        self.interval_spin = QDoubleSpinBox()
        self.interval_spin.setRange(0.0, 10000.0)
        self.interval_spin.setDecimals(3)
        self.interval_spin.setValue(10.0)

        self.seed_spin = QSpinBox()
        self.seed_spin.setRange(0, 9999999)
        self.seed_spin.setValue(1)

        self.rate_label = QLabel("TX target: 6400.0 B/s")

        self.stress_start_btn = QPushButton("Start Stress")
        self.stress_stop_btn = QPushButton("Stop")
        self.stress_stop_btn.setEnabled(False)

        self.send_hello_btn = QPushButton("Send HELLO")

        self.progress = QProgressBar()
        self.progress.setRange(0, 100)

        layout.addWidget(QLabel("Pattern:"))
        layout.addWidget(self.pattern_combo)

        layout.addWidget(QLabel("Size:"))
        layout.addWidget(self.size_spin)

        layout.addWidget(QLabel("Chunk:"))
        layout.addWidget(self.chunk_spin)

        layout.addWidget(QLabel("Interval ms:"))
        layout.addWidget(self.interval_spin)

        layout.addWidget(QLabel("Seed:"))
        layout.addWidget(self.seed_spin)

        layout.addWidget(self.rate_label)

        layout.addWidget(self.send_hello_btn)
        layout.addWidget(self.stress_start_btn)
        layout.addWidget(self.stress_stop_btn)
        layout.addWidget(self.progress, stretch=1)

        return group

    def _build_log_group(self) -> QGroupBox:
        group = QGroupBox("Raw Text Log")
        layout = QVBoxLayout(group)

        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)

        layout.addWidget(self.log_text)

        return group

    def _connect_signals(self) -> None:
        self.refresh_btn.clicked.connect(self._refresh_ports)
        self.connect_btn.clicked.connect(self._connect_serial)
        self.disconnect_btn.clicked.connect(self._disconnect_serial)

        self.send_hello_btn.clicked.connect(self._send_hello)

        self.stress_start_btn.clicked.connect(self._start_stress)
        self.stress_stop_btn.clicked.connect(self._stop_stress)

        self.chunk_spin.valueChanged.connect(self._update_target_rate_label)
        self.interval_spin.valueChanged.connect(self._update_target_rate_label)

        self.proto_ping_btn.clicked.connect(
            lambda: self._send_protocol_command(MCU_INFO_CMD_PING, "PING")
        )
        self.proto_version_btn.clicked.connect(
            lambda: self._send_protocol_command(MCU_INFO_CMD_GET_VERSION, "VERSION")
        )
        self.proto_status_btn.clicked.connect(
            lambda: self._send_protocol_command(MCU_INFO_CMD_GET_STATUS, "STATUS")
        )
        self.proto_time_btn.clicked.connect(
            lambda: self._send_protocol_command(MCU_INFO_CMD_GET_TIME_INFO, "TIME")
        )
        self.proto_reset_btn.clicked.connect(
            lambda: self._send_protocol_command(MCU_INFO_CMD_GET_RESET_INFO, "RESET_INFO")
        )
        self.proto_uart_btn.clicked.connect(
            lambda: self._send_protocol_command(MCU_INFO_CMD_GET_UART_STATS, "UART_STATS")
        )
        self.proto_app_btn.clicked.connect(
            lambda: self._send_protocol_command(MCU_INFO_CMD_GET_APP_STATS, "APP_STATS")
        )

        self.proto_auto_poll_check.stateChanged.connect(self._on_protocol_auto_poll_changed)
        self.proto_poll_interval_spin.valueChanged.connect(self._on_protocol_poll_interval_changed)

        self.serial_worker.connected.connect(self._on_connected)
        self.serial_worker.disconnected.connect(self._on_disconnected)
        self.serial_worker.error.connect(self._on_error)
        self.serial_worker.raw_line.connect(self._on_raw_line)
        self.serial_worker.uart_stat.connect(self._on_uart_stat)
        self.serial_worker.protocol_frame.connect(self._on_protocol_frame)
        self.serial_worker.protocol_event.connect(self._on_protocol_event)
        self.serial_worker.protocol_response.connect(self._on_protocol_response)
        self.serial_worker.protocol_nack.connect(self._on_protocol_nack)
        self.serial_worker.tx_bytes_written.connect(self._on_tx_bytes_written)
        self.serial_worker.tx_queue_size_changed.connect(self._on_tx_queue_size_changed)

    # -------------------------------------------------------------------------
    # Serial
    # -------------------------------------------------------------------------

    def _refresh_ports(self) -> None:
        self._append_log("[UI] Refresh ports")

        current = self.port_combo.currentText()
        self.port_combo.clear()

        ports = list_serial_ports()
        self.port_combo.addItems(ports)

        if current in ports:
            self.port_combo.setCurrentText(current)

        self._append_log(f"[UI] Ports: {ports}")

    def _connect_serial(self) -> None:
        self._append_log("[UI] Connect button clicked")
        print("[DEBUG] Connect button clicked")

        port = self.port_combo.currentText()
        baud = int(self.baud_combo.currentText())

        if not port:
            QMessageBox.warning(self, "Warning", "No serial port selected.")
            self._append_log("[ERROR] No serial port selected")
            return

        self._append_log(f"[UI] Connect requested: {port} @ {baud}")
        self.serial_worker.connect_port(port, baud)

    def _disconnect_serial(self) -> None:
        self._append_log("[UI] Disconnect button clicked")
        print("[DEBUG] Disconnect button clicked")

        if self.protocol_poll_timer.isActive():
            self.protocol_poll_timer.stop()
        self.proto_auto_poll_check.setChecked(False)

        self._stop_stress_silent()

        self.disconnect_btn.setEnabled(False)
        self.status_label.setText("Disconnecting...")

        self.serial_worker.disconnect_port()

    def _on_connected(self, msg: str) -> None:
        self.status_label.setText("Connected")
        self.connect_btn.setEnabled(False)
        self.disconnect_btn.setEnabled(True)
        self._append_log(f"[UI] {msg}")

    def _on_disconnected(self) -> None:
        self.status_label.setText("Disconnected")
        self.connect_btn.setEnabled(True)
        self.disconnect_btn.setEnabled(False)

        if self.protocol_poll_timer.isActive():
            self.protocol_poll_timer.stop()
        self.proto_auto_poll_check.setChecked(False)

        self._append_log("[UI] Disconnected")

    def _on_error(self, msg: str) -> None:
        self._append_log(f"[ERROR] {msg}")
        print(f"[ERROR] {msg}")

        self.connect_btn.setEnabled(True)
        self.disconnect_btn.setEnabled(False)
        self.status_label.setText("Error")

        if self.protocol_poll_timer.isActive():
            self.protocol_poll_timer.stop()
        self.proto_auto_poll_check.setChecked(False)

        self._stop_stress_silent()

    # -------------------------------------------------------------------------
    # RX telemetry
    # -------------------------------------------------------------------------

    def _on_raw_line(self, line: str) -> None:
        self._append_log(line)

    def _on_uart_stat(self, stat: UartStat) -> None:
        rx_rate = 0.0

        if self.last_rx_bytes is not None and self.last_host_time is not None:
            dt = stat.host_time - self.last_host_time
            if dt > 0:
                rx_rate = (stat.rx_bytes - self.last_rx_bytes) / dt

        self.last_rx_bytes = stat.rx_bytes
        self.last_host_time = stat.host_time

        self.metric_labels["t"].setText(str(stat.t))
        self.metric_labels["rx_bytes"].setText(str(stat.rx_bytes))
        self.metric_labels["rx_rate"].setText(f"{rx_rate:.1f}")
        self.metric_labels["avail"].setText(str(stat.avail))
        self.metric_labels["free"].setText(str(stat.free))
        self.metric_labels["rb_write"].setText(str(stat.rb_write))
        self.metric_labels["rb_read"].setText(str(stat.rb_read))
        self.metric_labels["overflow"].setText(str(stat.overflow))
        self.metric_labels["rb_overflow"].setText(str(stat.rb_overflow))
        self.metric_labels["high"].setText(str(stat.high))
        self.metric_labels["half"].setText(str(stat.half))
        self.metric_labels["full"].setText(str(stat.full))
        self.metric_labels["idle"].setText(str(stat.idle))
        self.metric_labels["err"].setText(str(stat.err))

        self.ring_widget.update_stats(
            available=stat.avail,
            free=stat.free,
            high_watermark=stat.high,
        )

        self.time_data.append(stat.host_time)
        self.avail_data.append(stat.avail)
        self.high_data.append(stat.high)
        self.rx_rate_data.append(rx_rate)

        if len(self.time_data) > 0:
            x = [t - self.time_data[0] for t in self.time_data]

            self.curve_avail.setData(x, list(self.avail_data))
            self.curve_high.setData(x, list(self.high_data))
            self.curve_rx_rate.setData(x, list(self.rx_rate_data))

    # -------------------------------------------------------------------------
    # Protocol monitor
    # -------------------------------------------------------------------------

    def _send_protocol_command(self, cmd: int, name: str) -> None:
        if not self.serial_worker.is_open():
            self._append_protocol_log(f"[ERROR] Port is not open, cannot send {name}")
            return

        try:
            seq, frame = self.serial_worker.send_request(cmd)
        except Exception as exc:
            self._append_protocol_log(f"[ERROR] send {name} failed: {exc}")
            return

        self.protocol_tx_req_count += 1
        self.protocol_last_seq = seq
        self._update_protocol_labels()

        self._append_protocol_log(
            f"[TX] {name}: seq={seq}, cmd=0x{cmd:02X}, bytes={frame.hex(' ').upper()}"
        )

    def _on_protocol_auto_poll_changed(self) -> None:
        if self.proto_auto_poll_check.isChecked():
            if not self.serial_worker.is_open():
                self._append_protocol_log("[ERROR] Port is not open, auto poll disabled")
                self.proto_auto_poll_check.setChecked(False)
                return

            interval = self.proto_poll_interval_spin.value()
            self.protocol_poll_timer.start(interval)
            self._append_protocol_log(f"[UI] Auto poll started, interval={interval} ms")
        else:
            if self.protocol_poll_timer.isActive():
                self.protocol_poll_timer.stop()
            self._append_protocol_log("[UI] Auto poll stopped")

    def _on_protocol_poll_interval_changed(self) -> None:
        if self.protocol_poll_timer.isActive():
            self.protocol_poll_timer.start(self.proto_poll_interval_spin.value())

    def _protocol_auto_poll_once(self) -> None:
        poll_items = [
            (MCU_INFO_CMD_GET_UART_STATS, "UART_STATS"),
            (MCU_INFO_CMD_GET_APP_STATS, "APP_STATS"),
            (MCU_INFO_CMD_GET_TIME_INFO, "TIME"),
            (MCU_INFO_CMD_GET_STATUS, "STATUS"),
            (MCU_INFO_CMD_GET_RESET_INFO, "RESET_INFO"),
        ]

        cmd, name = poll_items[self.protocol_auto_poll_index % len(poll_items)]
        self.protocol_auto_poll_index += 1

        self._send_protocol_command(cmd, name)

    def _on_protocol_frame(self, frame: ProtoFrame) -> None:
        self.protocol_frame_count += 1
        self._update_protocol_labels()

    def _on_protocol_response(self, frame: ProtoFrame) -> None:
        self.protocol_resp_count += 1
        self._update_protocol_labels()

        text = frame.payload_ascii()
        self._append_protocol_log(
            f"[RESP] seq={frame.seq}, cmd=0x{frame.cmd:02X}, "
            f"len={len(frame.payload)}, payload=[{text}], raw={frame.payload_hex()}"
        )

    def _on_protocol_nack(self, frame: ProtoFrame) -> None:
        self.protocol_nack_count += 1
        self._update_protocol_labels()

        err_code = frame.payload[0] if len(frame.payload) >= 1 else 0xFF

        self._append_protocol_log(
            f"[NACK] seq={frame.seq}, cmd=0x{frame.cmd:02X}, "
            f"err=0x{err_code:02X}({error_name(err_code)}), "
            f"payload={frame.payload_hex()}"
        )

    def _on_protocol_event(self, frame: ProtoFrame) -> None:
        self.protocol_event_count += 1
        self._update_protocol_labels()

        event = decode_mcu_info_event(frame)
        self._append_event_log(
            f"[EVENT] seq={frame.seq}, cmd=0x{frame.cmd:02X}({event_name(frame.cmd)}), "
            f"{event_summary(event)}"
        )

    def _update_protocol_labels(self) -> None:
        self.protocol_labels["frames"].setText(str(self.protocol_frame_count))
        self.protocol_labels["resp"].setText(str(self.protocol_resp_count))
        self.protocol_labels["nack"].setText(str(self.protocol_nack_count))
        self.protocol_labels["event"].setText(str(self.protocol_event_count))
        self.protocol_labels["tx_req"].setText(str(self.protocol_tx_req_count))
        self.protocol_labels["last_seq"].setText(str(self.protocol_last_seq))

    # -------------------------------------------------------------------------
    # TX / stress
    # -------------------------------------------------------------------------

    def _update_target_rate_label(self) -> None:
        chunk = self.chunk_spin.value()
        interval_ms = self.interval_spin.value()

        if interval_ms <= 0.0:
            self.rate_label.setText("TX target: as fast as possible")
            return

        target_rate = chunk * 1000.0 / interval_ms
        self.rate_label.setText(f"TX target: {target_rate:.1f} B/s")

    def _send_hello(self) -> None:
        self._append_log("[UI] Send HELLO clicked")

        try:
            self.serial_worker.enqueue_tx(b"hello\r\n")
        except Exception as exc:
            self._append_log(f"[ERROR] send hello failed: {exc}")

    def _start_stress(self) -> None:
        if self.stress_running:
            self._append_log("[UI] Stress already running")
            return

        self._append_log("[UI] Start Stress clicked")

        pattern = self.pattern_combo.currentText()
        size = self.size_spin.value()
        seed = self.seed_spin.value()

        try:
            self.stress_data = make_pattern(pattern, size, seed)
        except Exception as exc:
            self._append_log(f"[ERROR] make pattern failed: {exc}")
            return

        self.stress_total = len(self.stress_data)
        self.stress_enqueued = 0
        self.stress_tx_written = 0
        self.stress_start_time = time.time()
        self.stress_chunk = self.chunk_spin.value()
        self.stress_running = True

        self.progress.setValue(0)
        self.stress_start_btn.setEnabled(False)
        self.stress_stop_btn.setEnabled(True)

        self._append_log(
            f"[UI] Stress start: pattern={pattern}, "
            f"size={self.stress_total}, "
            f"chunk={self.stress_chunk}, "
            f"interval_ms={self.interval_spin.value()}, "
            f"seed={seed}"
        )

        try:
            self.serial_worker.clear_tx_queue()
        except Exception:
            pass

        if self.stress_total == 0:
            self._finish_stress()
            return

        self._stress_enqueue_next_chunk()

        if self.stress_running:
            interval_ms = max(0, int(self.interval_spin.value()))
            self.stress_timer.start(interval_ms)

    def _stress_enqueue_next_chunk(self) -> None:
        if not self.stress_running:
            return

        if self.stress_enqueued >= self.stress_total:
            self.stress_timer.stop()
            return

        end = min(self.stress_enqueued + self.stress_chunk, self.stress_total)
        chunk_data = self.stress_data[self.stress_enqueued:end]

        try:
            self.serial_worker.enqueue_tx(chunk_data)
        except Exception as exc:
            self._append_log(f"[ERROR] enqueue chunk failed: {exc}")
            self._stop_stress()
            return

        self.stress_enqueued = end

        if self.stress_enqueued >= self.stress_total:
            self.stress_timer.stop()

    def _stop_stress(self) -> None:
        self._append_log("[UI] Stop Stress clicked")

        if self.stress_running:
            elapsed = time.time() - self.stress_start_time if self.stress_start_time > 0 else 0.0
            rate = self.stress_tx_written / elapsed if elapsed > 0 else 0.0

            self._append_log(
                f"[UI] Stress stopped: written={self.stress_tx_written}/{self.stress_total}, "
                f"enqueued={self.stress_enqueued}/{self.stress_total}, "
                f"elapsed={elapsed:.3f}s, tx_rate={rate:.1f} B/s"
            )

        self._stop_stress_silent()

    def _stop_stress_silent(self) -> None:
        if self.stress_timer.isActive():
            self.stress_timer.stop()

        try:
            self.serial_worker.clear_tx_queue()
        except Exception:
            pass

        self.stress_running = False
        self.stress_start_btn.setEnabled(True)
        self.stress_stop_btn.setEnabled(False)
        self.stress_total = 0
        self.stress_enqueued = 0
        self.stress_tx_written = 0
        self.stress_start_time = 0.0
        self.progress.setValue(0)

    def _finish_stress(self) -> None:
        elapsed = time.time() - self.stress_start_time if self.stress_start_time > 0 else 0.0
        rate = self.stress_tx_written / elapsed if elapsed > 0 else 0.0

        self._append_log(
            f"[UI] Stress finished: written={self.stress_tx_written}, "
            f"elapsed={elapsed:.3f}s, tx_rate={rate:.1f} B/s"
        )

        self._stop_stress_silent()

    def _on_tx_bytes_written(self, written: int) -> None:
        self.metric_labels["tx_written"].setText(str(written))

        if not self.stress_running:
            return

        self.stress_tx_written += written

        if self.stress_total > 0:
            percent = int(self.stress_tx_written * 100 / self.stress_total)
            self.progress.setValue(min(100, percent))

        if self.stress_tx_written >= self.stress_total:
            self._finish_stress()

    def _on_tx_queue_size_changed(self, size: int) -> None:
        pass

    # -------------------------------------------------------------------------
    # Log
    # -------------------------------------------------------------------------

    def _append_log(self, text: str) -> None:
        print(text)
        self.log_text.append(text)
        self.log_text.moveCursor(QTextCursor.End)

    def _append_protocol_log(self, text: str) -> None:
        print(text)
        self.protocol_log_text.append(text)
        self.protocol_log_text.moveCursor(QTextCursor.End)

    def _append_event_log(self, text: str) -> None:
        print(text)
        self.event_log_text.append(text)
        self.event_log_text.moveCursor(QTextCursor.End)