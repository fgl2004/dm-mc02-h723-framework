from __future__ import annotations

from PySide6.QtCore import Qt, QRectF
from PySide6.QtGui import QPainter, QColor, QPen, QFont
from PySide6.QtWidgets import QWidget


class RingBufferWidget(QWidget):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)

        self.capacity = 0
        self.available = 0
        self.free = 0
        self.high_watermark = 0

        self.setMinimumSize(240, 240)
        self.setMaximumSize(360, 360)

    def update_stats(self, available: int, free: int, high_watermark: int) -> None:
        self.available = max(0, int(available))
        self.free = max(0, int(free))
        self.capacity = max(0, self.available + self.free)
        self.high_watermark = max(0, int(high_watermark))
        self.update()

    def paintEvent(self, event) -> None:
        del event

        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        w = self.width()
        h = self.height()
        side = min(w, h) - 50

        cx = w / 2
        cy = h / 2

        rect = QRectF(cx - side / 2, cy - side / 2, side, side)

        bg_color = QColor(210, 210, 210)
        used_color = QColor(0, 170, 90)
        high_color = QColor(255, 160, 0)
        text_color = QColor(40, 40, 40)
        sub_text_color = QColor(90, 90, 90)

        start_angle = 90 * 16
        full_span = -360 * 16

        pen_bg = QPen(bg_color, 18)
        painter.setPen(pen_bg)
        painter.drawArc(rect, start_angle, full_span)

        if self.capacity > 0:
            used_ratio = min(1.0, self.available / self.capacity)
            high_ratio = min(1.0, self.high_watermark / self.capacity)
        else:
            used_ratio = 0.0
            high_ratio = 0.0

        pen_used = QPen(used_color, 18)
        painter.setPen(pen_used)
        painter.drawArc(rect, start_angle, int(full_span * used_ratio))

        rect_high = QRectF(
            cx - side / 2 - 12,
            cy - side / 2 - 12,
            side + 24,
            side + 24,
        )

        pen_high_bg = QPen(QColor(230, 230, 230), 6)
        painter.setPen(pen_high_bg)
        painter.drawArc(rect_high, start_angle, full_span)

        pen_high = QPen(high_color, 6)
        painter.setPen(pen_high)
        painter.drawArc(rect_high, start_angle, int(full_span * high_ratio))

        painter.setPen(text_color)

        title_font = QFont()
        title_font.setPointSize(11)
        title_font.setBold(True)
        painter.setFont(title_font)
        painter.drawText(
            self.rect().adjusted(0, -62, 0, 0),
            Qt.AlignCenter,
            "RingBuffer",
        )

        info_font = QFont()
        info_font.setPointSize(15)
        info_font.setBold(True)
        painter.setFont(info_font)

        ratio_percent = used_ratio * 100.0
        center_text = f"{self.available} / {self.capacity}\n{ratio_percent:.1f}%"
        painter.drawText(self.rect(), Qt.AlignCenter, center_text)

        painter.setPen(sub_text_color)

        small_font = QFont()
        small_font.setPointSize(10)
        painter.setFont(small_font)

        bottom_text = f"Free: {self.free}\nHigh: {self.high_watermark}"
        painter.drawText(
            self.rect().adjusted(0, 85, 0, 0),
            Qt.AlignCenter,
            bottom_text,
        )