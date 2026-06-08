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
    QTabWidget,
)

import pyqtgraph as pg

from h7_proto.constants import (
    DIAG_CMD_GET_BUFFER_STATS,
    DIAG_CMD_GET_ERROR_COUNTERS,
    DIAG_CMD_GET_HEALTH,
    DIAG_CMD_GET_LAST_RECORDS,
    DIAG_CMD_GET_TIMING_STATS,
    DIAG_CMD_GET_PIPELINE_STATS,
    DIAG_CMD_CLEAR_COUNTERS,
    MCU_INFO_CMD_GET_APP_STATS,
    MCU_INFO_CMD_GET_COMMAND_STATS,
    MCU_INFO_CMD_GET_RESET_INFO,
    MCU_INFO_CMD_GET_STATUS,
    MCU_INFO_CMD_GET_TIME_INFO,
    MCU_INFO_CMD_GET_UART_STATS,
    MCU_INFO_CMD_GET_VERSION,
    MCU_INFO_CMD_PING,
    IMU_APP_ID,
    IMU_EVENT_ATTITUDE,
    IMU_EVENT_ERROR,
    IMU_EVENT_RAW_SAMPLE,
    IMU_EVENT_STARTED,
    IMU_EVENT_STOPPED,
    IMU_CMD_GET_RAW,
    IMU_CMD_GET_FILTERED,
    IMU_CMD_GET_STATUS,
    IMU_CMD_GET_ALGO_STATS,
    IMU_CMD_SET_SAMPLE_RATE,
    IMU_CMD_START_STREAM,
    IMU_CMD_STOP_STREAM,
    IMU_CMD_START,
    IMU_CMD_STOP,
    IMU_CMD_GET_ATTITUDE,
    IMU_CMD_CLEAR_STATS,
    STREAM_MANAGER_DATA_CMD,
    BLOCK_MANAGER_DATA_CMD,
    STREAM_CHANNEL_IMU,
    TYPE_DATA,
    TYPE_ACK,
    TYPE_WINDOW_ACK,
    block_op_name,
    stream_channel_name,
    event_name,
    error_name,
)
from h7_proto.event import decode_mcu_info_event, event_summary
from h7_proto.frame import ProtoFrame
from h7_proto.stream_client import StreamClient, StreamSample, stream_sample_summary
from h7_proto.block_client import BlockClient, block_packet_summary

from .patterns import AVAILABLE_PATTERNS, make_pattern
from .serial_worker import SerialWorker, list_serial_ports
from .telemetry import UartStat
from .ring_buffer_widget import RingBufferWidget
from .imu_3d_widget import Imu3DWidget
from .storage_tab import StorageTab



class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()

        print("[DEBUG] MainWindow.__init__()")

        self.setWindowTitle("H7 UART DMA RX + Protocol Monitor")
        self.resize(1500, 940)

        self.serial_worker = SerialWorker()

        # Protocol semantic clients.
        # SerialWorker owns transport + byte stream parsing.
        # MainWindow owns UI routing for command/event/stream/block frames.
        self.stream_client = StreamClient()
        self.block_client = BlockClient()

        self.stats_history_len = 300
        self.time_data = deque(maxlen=self.stats_history_len)
        self.avail_data = deque(maxlen=self.stats_history_len)
        self.high_data = deque(maxlen=self.stats_history_len)
        self.rx_rate_data = deque(maxlen=self.stats_history_len)

        # IMU dashboard history
        self.imu_history_len = 500
        self.imu_t0: float | None = None
        self.imu_time_data = deque(maxlen=self.imu_history_len)
        self.imu_att_time_data = deque(maxlen=self.imu_history_len)
        self.imu_ax_data = deque(maxlen=self.imu_history_len)
        self.imu_ay_data = deque(maxlen=self.imu_history_len)
        self.imu_az_data = deque(maxlen=self.imu_history_len)
        self.imu_gx_data = deque(maxlen=self.imu_history_len)
        self.imu_gy_data = deque(maxlen=self.imu_history_len)
        self.imu_gz_data = deque(maxlen=self.imu_history_len)
        self.imu_roll_data = deque(maxlen=self.imu_history_len)
        self.imu_pitch_data = deque(maxlen=self.imu_history_len)
        self.imu_yaw_data = deque(maxlen=self.imu_history_len)

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
        self.protocol_data_count = 0
        self.protocol_ack_count = 0
        self.protocol_window_ack_count = 0
        self.protocol_stream_count = 0
        self.protocol_block_count = 0
        self.protocol_unknown_data_count = 0
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

        self.main_tabs = QTabWidget()
        self.main_tabs.addTab(self._build_protocol_diagnostic_tab(), "Protocol / Diagnostic")
        self.main_tabs.addTab(self._build_imu_tab(), "IMU Dashboard")
        self.storage_tab = StorageTab(self.serial_worker)
        self.main_tabs.addTab(self.storage_tab, "Storage Explorer")
        layout.addWidget(self.main_tabs, stretch=1)

        self.setCentralWidget(root)

    def _build_protocol_diagnostic_tab(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)

        layout.addWidget(self._build_metrics_group())

        middle_layout = QHBoxLayout()
        middle_layout.addWidget(self._build_visual_group(), stretch=3)
        middle_layout.addWidget(self._build_protocol_group(), stretch=2)
        layout.addLayout(middle_layout, stretch=4)

        layout.addWidget(self._build_stress_group())
        layout.addWidget(self._build_log_group(), stretch=2)

        return page

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
            "data",
            "ack",
            "win_ack",
            "stream",
            "block",
            "unknown_data",
            "tx_req",
            "last_seq",
        ]

        for idx, name in enumerate(names):
            row = idx // 4
            col = (idx % 4) * 2

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
        self.proto_cmd_stats_btn = QPushButton("COMMAND_STATS")

        self.diag_health_btn = QPushButton("HEALTH")
        self.diag_error_btn = QPushButton("ERRORS")
        self.diag_buffer_btn = QPushButton("BUFFERS")
        self.diag_last_btn = QPushButton("LAST_RECORDS")
        self.diag_timing_btn = QPushButton("TIMING")
        self.diag_pipeline_btn = QPushButton("PIPELINE")
        self.diag_clear_btn = QPushButton("CLEAR_COUNTERS")

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
        cmd_layout.addWidget(self.proto_cmd_stats_btn, 2, 1)

        cmd_layout.addWidget(self.diag_health_btn, 3, 0)
        cmd_layout.addWidget(self.diag_error_btn, 3, 1)
        cmd_layout.addWidget(self.diag_buffer_btn, 3, 2)

        cmd_layout.addWidget(self.diag_last_btn, 4, 0)
        cmd_layout.addWidget(self.diag_timing_btn, 4, 1)
        cmd_layout.addWidget(self.diag_pipeline_btn, 4, 2)

        cmd_layout.addWidget(self.diag_clear_btn, 5, 0)

        cmd_layout.addWidget(self.proto_auto_poll_check, 6, 0)
        cmd_layout.addWidget(QLabel("Interval:"), 6, 1)
        cmd_layout.addWidget(self.proto_poll_interval_spin, 6, 2)

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

    def _build_imu_tab(self) -> QWidget:
        page = QWidget()
        layout = QVBoxLayout(page)

        top_layout = QHBoxLayout()
        top_layout.addWidget(self._build_imu_control_group(), stretch=1)
        top_layout.addWidget(self._build_imu_numeric_group(), stretch=3)
        layout.addLayout(top_layout, stretch=1)

        middle_layout = QHBoxLayout()
        middle_layout.addWidget(self._build_imu_scope_group(), stretch=3)
        middle_layout.addWidget(self._build_imu_3d_group(), stretch=2)
        layout.addLayout(middle_layout, stretch=4)

        layout.addWidget(self._build_imu_log_group(), stretch=1)

        return page

    def _build_imu_control_group(self) -> QGroupBox:
        group = QGroupBox("IMU Control")
        layout = QGridLayout(group)

        self.imu_start_btn = QPushButton("START")
        self.imu_stop_btn = QPushButton("STOP")
        self.imu_start_stream_btn = QPushButton("START STREAM")
        self.imu_stop_stream_btn = QPushButton("STOP STREAM")

        self.imu_status_btn = QPushButton("GET STATUS")
        self.imu_raw_btn = QPushButton("GET RAW")
        self.imu_filtered_btn = QPushButton("GET FILTERED")
        self.imu_attitude_btn = QPushButton("GET ATTITUDE")
        self.imu_stats_btn = QPushButton("GET STATS")
        self.imu_clear_btn = QPushButton("CLEAR STATS")

        self.imu_period_spin = QSpinBox()
        self.imu_period_spin.setRange(1, 1000)
        self.imu_period_spin.setValue(10)
        self.imu_period_spin.setSuffix(" ms")
        self.imu_set_period_btn = QPushButton("SET PERIOD")

        layout.addWidget(self.imu_start_btn, 0, 0)
        layout.addWidget(self.imu_stop_btn, 0, 1)
        layout.addWidget(self.imu_start_stream_btn, 1, 0)
        layout.addWidget(self.imu_stop_stream_btn, 1, 1)

        layout.addWidget(self.imu_status_btn, 2, 0)
        layout.addWidget(self.imu_raw_btn, 2, 1)
        layout.addWidget(self.imu_filtered_btn, 3, 0)
        layout.addWidget(self.imu_attitude_btn, 3, 1)
        layout.addWidget(self.imu_stats_btn, 4, 0)
        layout.addWidget(self.imu_clear_btn, 4, 1)

        layout.addWidget(QLabel("Sample period:"), 5, 0)
        layout.addWidget(self.imu_period_spin, 5, 1)
        layout.addWidget(self.imu_set_period_btn, 6, 0, 1, 2)

        return group

    def _build_imu_numeric_group(self) -> QGroupBox:
        group = QGroupBox("IMU Numeric Data")
        layout = QGridLayout(group)

        self.imu_value_labels: dict[str, QLabel] = {}

        names = [
            "state", "started", "stream", "err", "sample", "period",
            "ax", "ay", "az", "gx", "gy", "gz", "temp", "tick",
            "ax_mg", "ay_mg", "az_mg", "gx_mdps", "gy_mdps", "gz_mdps", "temp_mc",
            "roll", "pitch", "yaw", "q0", "q1", "q2", "q3",
            "read_err", "att", "att_err", "evt", "drop", "read_us", "algo_us", "run_us",
        ]

        for idx, name in enumerate(names):
            row = idx // 6
            col = (idx % 6) * 2
            layout.addWidget(QLabel(name + ":"), row, col)
            value_label = QLabel("-")
            value_label.setMinimumWidth(72)
            value_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
            layout.addWidget(value_label, row, col + 1)
            self.imu_value_labels[name] = value_label

        return group

    def _build_imu_scope_group(self) -> QGroupBox:
        group = QGroupBox("IMU Attitude Oscilloscope")
        layout = QVBoxLayout(group)

        self.imu_scope_window_spin = QSpinBox()
        self.imu_scope_window_spin.setRange(3, 120)
        self.imu_scope_window_spin.setValue(15)
        self.imu_scope_window_spin.setSuffix(" s")

        scope_header = QHBoxLayout()
        scope_header.addWidget(QLabel("Display window:"))
        scope_header.addWidget(self.imu_scope_window_spin)
        scope_header.addStretch(1)
        layout.addLayout(scope_header)

        self.imu_plot_attitude = pg.PlotWidget(title="Attitude: roll / pitch / yaw (deg)")
        self.imu_plot_attitude.showGrid(x=True, y=True)
        self.imu_plot_attitude.setLabel("bottom", "time", units="s")
        self.imu_plot_attitude.setLabel("left", "angle", units="deg")
        self.imu_plot_attitude.setMouseEnabled(x=True, y=True)
        self.imu_plot_attitude.enableAutoRange(axis=pg.ViewBox.XYAxes, enable=False)

        self.imu_plot_attitude.addLegend(offset=(10, 10))
        self.imu_curve_roll = self.imu_plot_attitude.plot(
            name="roll",
            pen=pg.mkPen("#ff4d4d", width=2),
        )
        self.imu_curve_pitch = self.imu_plot_attitude.plot(
            name="pitch",
            pen=pg.mkPen("#4dff88", width=2),
        )
        self.imu_curve_yaw = self.imu_plot_attitude.plot(
            name="yaw",
            pen=pg.mkPen("#66a3ff", width=2),
        )

        layout.addWidget(self.imu_plot_attitude, stretch=1)

        return group

    def _build_imu_3d_group(self) -> QGroupBox:
        group = QGroupBox("3D Attitude View")
        layout = QVBoxLayout(group)

        self.imu_3d_widget = Imu3DWidget()
        layout.addWidget(self.imu_3d_widget, stretch=1)

        return group

    def _build_imu_log_group(self) -> QGroupBox:
        group = QGroupBox("IMU Log")
        layout = QVBoxLayout(group)

        self.imu_log_text = QTextEdit()
        self.imu_log_text.setReadOnly(True)
        self.imu_log_text.setMinimumHeight(120)

        layout.addWidget(self.imu_log_text)
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
        self.proto_cmd_stats_btn.clicked.connect(
            lambda: self._send_protocol_command(MCU_INFO_CMD_GET_COMMAND_STATS, "COMMAND_STATS")
        )
        self.diag_health_btn.clicked.connect(
            lambda: self._send_protocol_command(DIAG_CMD_GET_HEALTH, "HEALTH")
        )
        self.diag_error_btn.clicked.connect(
            lambda: self._send_protocol_command(DIAG_CMD_GET_ERROR_COUNTERS, "ERRORS")
        )
        self.diag_buffer_btn.clicked.connect(
            lambda: self._send_protocol_command(DIAG_CMD_GET_BUFFER_STATS, "BUFFERS")
        )
        self.diag_last_btn.clicked.connect(
            lambda: self._send_protocol_command(DIAG_CMD_GET_LAST_RECORDS, "LAST_RECORDS")
        )
        self.diag_timing_btn.clicked.connect(
            lambda: self._send_protocol_command(DIAG_CMD_GET_TIMING_STATS, "TIMING")
        )
        self.diag_pipeline_btn.clicked.connect(
            lambda: self._send_protocol_command(DIAG_CMD_GET_PIPELINE_STATS, "PIPELINE")
        )
        self.diag_clear_btn.clicked.connect(
            lambda: self._send_protocol_command(DIAG_CMD_CLEAR_COUNTERS, "CLEAR_COUNTERS")
        )

        self.imu_start_btn.clicked.connect(
            lambda: self._send_protocol_command(IMU_CMD_START, "IMU_START")
        )
        self.imu_stop_btn.clicked.connect(
            lambda: self._send_protocol_command(IMU_CMD_STOP, "IMU_STOP")
        )
        self.imu_start_stream_btn.clicked.connect(
            lambda: self._send_protocol_command(IMU_CMD_START_STREAM, "IMU_START_STREAM")
        )
        self.imu_stop_stream_btn.clicked.connect(
            lambda: self._send_protocol_command(IMU_CMD_STOP_STREAM, "IMU_STOP_STREAM")
        )
        self.imu_status_btn.clicked.connect(
            lambda: self._send_protocol_command(IMU_CMD_GET_STATUS, "IMU_GET_STATUS")
        )
        self.imu_raw_btn.clicked.connect(
            lambda: self._send_protocol_command(IMU_CMD_GET_RAW, "IMU_GET_RAW")
        )
        self.imu_filtered_btn.clicked.connect(
            lambda: self._send_protocol_command(IMU_CMD_GET_FILTERED, "IMU_GET_FILTERED")
        )
        self.imu_attitude_btn.clicked.connect(
            lambda: self._send_protocol_command(IMU_CMD_GET_ATTITUDE, "IMU_GET_ATTITUDE")
        )
        self.imu_stats_btn.clicked.connect(
            lambda: self._send_protocol_command(IMU_CMD_GET_ALGO_STATS, "IMU_GET_ALGO_STATS")
        )
        self.imu_clear_btn.clicked.connect(
            lambda: self._send_protocol_command(IMU_CMD_CLEAR_STATS, "IMU_CLEAR_STATS")
        )
        self.imu_set_period_btn.clicked.connect(self._send_imu_set_period)

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
        self.serial_worker.protocol_data.connect(self._on_protocol_data)
        self.serial_worker.protocol_ack.connect(self._on_protocol_ack)
        self.serial_worker.protocol_window_ack.connect(self._on_protocol_window_ack)
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

    def _send_protocol_command(self, cmd: int, name: str, payload: bytes = b"") -> None:
        if not self.serial_worker.is_open():
            self._append_protocol_log(f"[ERROR] Port is not open, cannot send {name}")
            return

        try:
            seq, frame = self.serial_worker.send_request(cmd, payload)
        except Exception as exc:
            self._append_protocol_log(f"[ERROR] send {name} failed: {exc}")
            return

        self.protocol_tx_req_count += 1
        self.protocol_last_seq = seq
        self._update_protocol_labels()

        payload_text = payload.decode("utf-8", errors="replace") if payload else ""
        if payload_text:
            self._append_protocol_log(
                f"[TX] {name}: seq={seq}, cmd=0x{cmd:02X}, payload=[{payload_text}], "
                f"bytes={frame.hex(' ').upper()}"
            )
        else:
            self._append_protocol_log(
                f"[TX] {name}: seq={seq}, cmd=0x{cmd:02X}, bytes={frame.hex(' ').upper()}"
            )

    def _send_imu_set_period(self) -> None:
        period_ms = self.imu_period_spin.value()
        payload = str(period_ms).encode("ascii")
        self._send_protocol_command(IMU_CMD_SET_SAMPLE_RATE, "IMU_SET_SAMPLE_RATE", payload)

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

        self._handle_imu_response(frame)
        if hasattr(self, "storage_tab"):
            self.storage_tab.handle_response(frame)

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

        self._handle_imu_event(event)

    def _on_protocol_data(self, frame: ProtoFrame) -> None:
        self.protocol_data_count += 1

        if frame.cmd == STREAM_MANAGER_DATA_CMD:
            sample = self.stream_client.handle_frame(frame)
            if sample is None:
                self.protocol_unknown_data_count += 1
                self._append_protocol_log(
                    f"[DATA][STREAM][BAD] seq={frame.seq}, len={len(frame.payload)}, raw={frame.payload_hex()}"
                )
            else:
                self.protocol_stream_count += 1
                self._handle_stream_sample(sample)

        elif frame.cmd == BLOCK_MANAGER_DATA_CMD:
            packet = self.block_client.handle_frame(frame)
            if packet is None:
                self.protocol_unknown_data_count += 1
                self._append_protocol_log(
                    f"[DATA][BLOCK][BAD] seq={frame.seq}, len={len(frame.payload)}, raw={frame.payload_hex()}"
                )
            else:
                self.protocol_block_count += 1
                self._append_protocol_log(
                    f"[DATA][BLOCK] {block_packet_summary(packet)}"
                )
                if hasattr(self, "storage_tab"):
                    self.storage_tab.handle_block_packet(packet)

        else:
            self.protocol_unknown_data_count += 1
            self._append_protocol_log(
                f"[DATA][UNKNOWN] seq={frame.seq}, cmd=0x{frame.cmd:02X}, "
                f"flags=0x{frame.flags:02X}, len={len(frame.payload)}, raw={frame.payload_hex()}"
            )

        self._update_protocol_labels()

    def _on_protocol_ack(self, frame: ProtoFrame) -> None:
        self.protocol_ack_count += 1

        if frame.cmd == BLOCK_MANAGER_DATA_CMD:
            packet = self.block_client.handle_frame(frame)
            if packet is not None:
                self.protocol_block_count += 1
                self._append_protocol_log(
                    f"[ACK][BLOCK] {block_packet_summary(packet)}"
                )
                if hasattr(self, "storage_tab"):
                    self.storage_tab.handle_block_packet(packet)
            else:
                self.protocol_unknown_data_count += 1
                self._append_protocol_log(
                    f"[ACK][BLOCK][BAD] seq={frame.seq}, len={len(frame.payload)}, raw={frame.payload_hex()}"
                )
        else:
            self._append_protocol_log(
                f"[ACK] seq={frame.seq}, cmd=0x{frame.cmd:02X}, "
                f"len={len(frame.payload)}, raw={frame.payload_hex()}"
            )

        self._update_protocol_labels()

    def _on_protocol_window_ack(self, frame: ProtoFrame) -> None:
        self.protocol_window_ack_count += 1

        if frame.cmd == BLOCK_MANAGER_DATA_CMD:
            packet = self.block_client.handle_frame(frame)
            if packet is not None:
                self.protocol_block_count += 1
                self._append_protocol_log(
                    f"[WINDOW_ACK][BLOCK] {block_packet_summary(packet)}"
                )
                if hasattr(self, "storage_tab"):
                    self.storage_tab.handle_block_packet(packet)
            else:
                self._append_protocol_log(
                    f"[WINDOW_ACK][BLOCK] seq={frame.seq}, len={len(frame.payload)}, raw={frame.payload_hex()}"
                )
        else:
            self._append_protocol_log(
                f"[WINDOW_ACK] seq={frame.seq}, cmd=0x{frame.cmd:02X}, "
                f"len={len(frame.payload)}, raw={frame.payload_hex()}"
            )

        self._update_protocol_labels()

    def _handle_stream_sample(self, sample: StreamSample) -> None:
        """
        Main path for MCU -> PC stream DATA.

        Most stream samples should not flood the text widget. We update counters
        and IMU dashboard every sample, but only log the first few and then
        periodic samples.
        """
        if sample.channel_id == STREAM_CHANNEL_IMU:
            self._handle_imu_stream_sample(sample)

        if self.protocol_stream_count <= 5 or (self.protocol_stream_count % 50) == 0:
            self._append_protocol_log(
                f"[DATA][STREAM] {stream_sample_summary(sample)}; {self.stream_client.summary()}"
            )

    def _handle_imu_stream_sample(self, sample: StreamSample) -> None:
        """
        Compatible with two stream payload styles:
          1. ASCII key-value sample, e.g. b"r=123,p=456,y=789,t=1000"
          2. Binary/raw sample: only counters/tick are updated here.
        """
        text = sample.sample_ascii()
        data = self._parse_kv_payload(text)

        self._set_imu_value("stream", "1")
        self._set_imu_value("sample", sample.seq)
        self._set_imu_value("tick", sample.timestamp_ms)

        if data:
            # Event-style attitude payload: r/p/y in centi-degrees.
            if ("r" in data) or ("p" in data) or ("y" in data):
                self._update_imu_attitude_from_event(data, sample.timestamp_ms)

            # Response-style attitude payload: roll_cdeg/pitch_cdeg/yaw_cdeg.
            elif ("roll_cdeg" in data) or ("pitch_cdeg" in data) or ("yaw_cdeg" in data):
                self._update_imu_attitude_from_response(data)

            # Raw sample payload.
            elif any(key in data for key in ["ax", "ay", "az", "gx", "gy", "gz"]):
                self._update_imu_raw(data, source="STREAM")

            if self.protocol_stream_count <= 5 or (self.protocol_stream_count % 50) == 0:
                self._append_imu_log(
                    f"[STREAM] channel={sample.channel_name()}, seq={sample.seq}, "
                    f"tick={sample.timestamp_ms}, data=[{text}]"
                )
        else:
            if self.protocol_stream_count <= 5 or (self.protocol_stream_count % 50) == 0:
                self._append_imu_log(
                    f"[STREAM] channel={sample.channel_name()}, seq={sample.seq}, "
                    f"tick={sample.timestamp_ms}, len={sample.sample_len}, raw={sample.sample_hex()}"
                )

    def _update_protocol_labels(self) -> None:
        self.protocol_labels["frames"].setText(str(self.protocol_frame_count))
        self.protocol_labels["resp"].setText(str(self.protocol_resp_count))
        self.protocol_labels["nack"].setText(str(self.protocol_nack_count))
        self.protocol_labels["event"].setText(str(self.protocol_event_count))
        self.protocol_labels["data"].setText(str(self.protocol_data_count))
        self.protocol_labels["ack"].setText(str(self.protocol_ack_count))
        self.protocol_labels["win_ack"].setText(str(self.protocol_window_ack_count))
        self.protocol_labels["stream"].setText(str(self.protocol_stream_count))
        self.protocol_labels["block"].setText(str(self.protocol_block_count))
        self.protocol_labels["unknown_data"].setText(str(self.protocol_unknown_data_count))
        self.protocol_labels["tx_req"].setText(str(self.protocol_tx_req_count))
        self.protocol_labels["last_seq"].setText(str(self.protocol_last_seq))

    # -------------------------------------------------------------------------
    # IMU dashboard
    # -------------------------------------------------------------------------

    # -------------------------------------------------------------------------

    def _parse_kv_payload(self, text: str) -> dict[str, str]:
        data: dict[str, str] = {}

        for item in text.split(","):
            item = item.strip()
            if "=" not in item:
                continue

            key, value = item.split("=", 1)
            data[key.strip()] = value.strip()

        return data

    def _kv_int(self, data: dict[str, str], key: str, default: int = 0) -> int:
        try:
            return int(data.get(key, str(default)), 0)
        except ValueError:
            return default

    def _set_imu_value(self, key: str, value) -> None:
        label = self.imu_value_labels.get(key)
        if label is not None:
            label.setText(str(value))

    def _handle_imu_response(self, frame: ProtoFrame) -> None:
        if frame.cmd not in {
            IMU_CMD_GET_RAW,
            IMU_CMD_GET_FILTERED,
            IMU_CMD_GET_STATUS,
            IMU_CMD_GET_ALGO_STATS,
            IMU_CMD_SET_SAMPLE_RATE,
            IMU_CMD_START_STREAM,
            IMU_CMD_STOP_STREAM,
            IMU_CMD_START,
            IMU_CMD_STOP,
            IMU_CMD_GET_ATTITUDE,
            IMU_CMD_CLEAR_STATS,
        }:
            return

        text = frame.payload_ascii()
        data = self._parse_kv_payload(text)
        self._append_imu_log(f"[RESP] cmd=0x{frame.cmd:02X}, payload=[{text}]")

        if frame.cmd == IMU_CMD_GET_STATUS:
            self._update_imu_status(data)
        elif frame.cmd == IMU_CMD_GET_RAW:
            self._update_imu_raw(data, source="RESP")
        elif frame.cmd == IMU_CMD_GET_FILTERED:
            self._update_imu_filtered(data)
        elif frame.cmd == IMU_CMD_GET_ATTITUDE:
            self._update_imu_attitude_from_response(data)
        elif frame.cmd == IMU_CMD_GET_ALGO_STATS:
            self._update_imu_stats(data)
        elif frame.cmd == IMU_CMD_SET_SAMPLE_RATE:
            self._set_imu_value("period", data.get("period", "-"))
            self._append_imu_log(f"[IMU] sample period set: {text}")
        elif frame.cmd == IMU_CMD_START:
            self._set_imu_value("started", "1")
            self._append_imu_log("[IMU] started")
        elif frame.cmd == IMU_CMD_STOP:
            self._set_imu_value("started", "0")
            self._set_imu_value("stream", "0")
            self._append_imu_log("[IMU] stopped")
        elif frame.cmd == IMU_CMD_START_STREAM:
            self._set_imu_value("stream", "1")
            self._append_imu_log("[IMU] stream started")
        elif frame.cmd == IMU_CMD_STOP_STREAM:
            self._set_imu_value("stream", "0")
            self._append_imu_log("[IMU] stream stopped")
        elif frame.cmd == IMU_CMD_CLEAR_STATS:
            self._append_imu_log("[IMU] stats cleared")

    def _handle_imu_event(self, event) -> None:
        if event is None:
            return

        if event.app_id != IMU_APP_ID:
            return

        text = event.data_ascii()
        data = self._parse_kv_payload(text)

        self._append_imu_log(
            f"[EVENT] id=0x{event.event_id:02X}({event_name(event.event_id)}), "
            f"tick={event.tick_ms}, data=[{text}]"
        )

        if event.event_id == IMU_EVENT_ATTITUDE:
            self._update_imu_attitude_from_event(data, event.tick_ms)
        elif event.event_id == IMU_EVENT_STARTED:
            self._set_imu_value("started", "1")
        elif event.event_id == IMU_EVENT_STOPPED:
            self._set_imu_value("started", "0")
            self._set_imu_value("stream", "0")
        elif event.event_id == IMU_EVENT_ERROR:
            self._set_imu_value("err", text)
        elif event.event_id == IMU_EVENT_RAW_SAMPLE:
            self._update_imu_raw(data, source="EVENT")

    def _update_imu_status(self, data: dict[str, str]) -> None:
        for key in ["state", "started", "stream", "err", "sample", "period"]:
            if key in data:
                self._set_imu_value(key, data[key])

    def _update_imu_raw(self, data: dict[str, str], source: str) -> None:
        for key in ["ax", "ay", "az", "gx", "gy", "gz", "temp", "tick"]:
            if key in data:
                self._set_imu_value(key, data[key])

        ax = self._kv_int(data, "ax")
        ay = self._kv_int(data, "ay")
        az = self._kv_int(data, "az")
        gx = self._kv_int(data, "gx")
        gy = self._kv_int(data, "gy")
        gz = self._kv_int(data, "gz")

        t_ms = self._kv_int(data, "tick", int(time.time() * 1000))
        self._append_imu_sample_time(t_ms)

        self.imu_ax_data.append(ax)
        self.imu_ay_data.append(ay)
        self.imu_az_data.append(az)
        self.imu_gx_data.append(gx)
        self.imu_gy_data.append(gy)
        self.imu_gz_data.append(gz)

        self._append_imu_log(f"[IMU {source}] raw ax={ax}, ay={ay}, az={az}, gx={gx}, gy={gy}, gz={gz}")

    def _update_imu_filtered(self, data: dict[str, str]) -> None:
        for key in ["ax_mg", "ay_mg", "az_mg", "gx_mdps", "gy_mdps", "gz_mdps", "temp_mc", "tick"]:
            if key in data:
                self._set_imu_value(key, data[key])

    def _update_imu_attitude_from_response(self, data: dict[str, str]) -> None:
        roll = self._kv_int(data, "roll_cdeg") / 100.0
        pitch = self._kv_int(data, "pitch_cdeg") / 100.0
        yaw = self._kv_int(data, "yaw_cdeg") / 100.0

        self._set_imu_value("roll", f"{roll:.2f}")
        self._set_imu_value("pitch", f"{pitch:.2f}")
        self._set_imu_value("yaw", f"{yaw:.2f}")

        for key, scale in [("q0", 1000.0), ("q1", 1000.0), ("q2", 1000.0), ("q3", 1000.0)]:
            raw_key = key + "_m"
            if raw_key in data:
                self._set_imu_value(key, f"{self._kv_int(data, raw_key) / scale:.3f}")

        if "tick" in data:
            self._set_imu_value("tick", data["tick"])
            self._append_imu_attitude_time(self._kv_int(data, "tick"))
        else:
            self._append_imu_attitude_time(int(time.time() * 1000))

        self.imu_roll_data.append(roll)
        self.imu_pitch_data.append(pitch)
        self.imu_yaw_data.append(yaw)
        self._update_imu_plots()
        self._update_imu_3d_placeholder(roll, pitch, yaw)

    def _update_imu_attitude_from_event(self, data: dict[str, str], event_tick_ms: int) -> None:
        roll = self._kv_int(data, "r") / 100.0
        pitch = self._kv_int(data, "p") / 100.0
        yaw = self._kv_int(data, "y") / 100.0
        tick = self._kv_int(data, "t", event_tick_ms)

        self._set_imu_value("roll", f"{roll:.2f}")
        self._set_imu_value("pitch", f"{pitch:.2f}")
        self._set_imu_value("yaw", f"{yaw:.2f}")
        self._set_imu_value("tick", str(tick))

        self._append_imu_attitude_time(tick)
        self.imu_roll_data.append(roll)
        self.imu_pitch_data.append(pitch)
        self.imu_yaw_data.append(yaw)

        self._update_imu_plots()
        self._update_imu_3d_placeholder(roll, pitch, yaw)

    def _update_imu_stats(self, data: dict[str, str]) -> None:
        key_map = {
            "sample": "sample",
            "read_err": "read_err",
            "att": "att",
            "att_err": "att_err",
            "evt": "evt",
            "drop": "drop",
            "read_us": "read_us",
            "algo_us": "algo_us",
            "run_us": "run_us",
        }
        for src, dst in key_map.items():
            if src in data:
                self._set_imu_value(dst, data[src])

    def _append_imu_sample_time(self, tick_ms: int) -> None:
        t = tick_ms / 1000.0
        if self.imu_t0 is None:
            self.imu_t0 = t
        self.imu_time_data.append(t - self.imu_t0)

    def _append_imu_attitude_time(self, tick_ms: int) -> None:
        t = tick_ms / 1000.0
        if self.imu_t0 is None:
            self.imu_t0 = t
        self.imu_att_time_data.append(t - self.imu_t0)

    def _update_imu_plots(self) -> None:
        x = list(self.imu_att_time_data)
        roll = list(self.imu_roll_data)
        pitch = list(self.imu_pitch_data)
        yaw = list(self.imu_yaw_data)

        n = min(len(x), len(roll), len(pitch), len(yaw))
        if n <= 0:
            return

        x = x[-n:]
        roll = roll[-n:]
        pitch = pitch[-n:]
        yaw = yaw[-n:]

        self.imu_curve_roll.setData(x, roll)
        self.imu_curve_pitch.setData(x, pitch)
        self.imu_curve_yaw.setData(x, yaw)

        self._auto_range_imu_attitude_plot(x, roll, pitch, yaw)

    def _auto_range_imu_attitude_plot(self, x: list[float], roll: list[float], pitch: list[float], yaw: list[float]) -> None:
        if not x:
            return

        window_s = float(self.imu_scope_window_spin.value()) if hasattr(self, "imu_scope_window_spin") else 15.0
        x_max = x[-1]
        x_min = max(0.0, x_max - window_s)

        visible_values: list[float] = []
        for idx, xv in enumerate(x):
            if xv >= x_min:
                visible_values.append(roll[idx])
                visible_values.append(pitch[idx])
                visible_values.append(yaw[idx])

        if not visible_values:
            visible_values = roll + pitch + yaw

        y_min = min(visible_values)
        y_max = max(visible_values)

        if y_min == y_max:
            margin = 1.0
        else:
            margin = max(1.0, (y_max - y_min) * 0.18)

        if x_max <= x_min:
            x_max = x_min + 1.0

        self.imu_plot_attitude.setXRange(x_min, x_max, padding=0.02)
        self.imu_plot_attitude.setYRange(y_min - margin, y_max + margin, padding=0.0)

    def _update_imu_3d_placeholder(self, roll: float, pitch: float, yaw: float) -> None:
        if hasattr(self, "imu_3d_widget"):
            self.imu_3d_widget.set_euler(roll, pitch, yaw)

    def _append_imu_log(self, text: str) -> None:
        print(text)
        self.imu_log_text.append(text)
        self.imu_log_text.moveCursor(QTextCursor.End)

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