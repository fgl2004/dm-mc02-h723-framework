from __future__ import annotations

import math

import numpy as np
from PySide6.QtWidgets import QVBoxLayout, QWidget

import pyqtgraph.opengl as gl


class Imu3DWidget(QWidget):
    """
    Real 3D IMU attitude widget based on pyqtgraph.opengl.

    Public API:
        set_euler(roll_deg, pitch_deg, yaw_deg)

    Coordinate convention:
        roll  -> rotate around X axis
        pitch -> rotate around Y axis
        yaw   -> rotate around Z axis

    If the board rotates in the wrong direction, adjust sign/offset below.
    """

    ROLL_SIGN = 1.0
    PITCH_SIGN = 1.0
    YAW_SIGN = 1.0

    ROLL_OFFSET_DEG = 0.0
    PITCH_OFFSET_DEG = 0.0
    YAW_OFFSET_DEG = 0.0

    def __init__(self, parent=None) -> None:
        super().__init__(parent)

        self.roll_deg = 0.0
        self.pitch_deg = 0.0
        self.yaw_deg = 0.0

        self.setMinimumHeight(430)
        self.setMinimumWidth(430)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        self.view = gl.GLViewWidget()
        self.view.setCameraPosition(distance=9.0, elevation=22.0, azimuth=45.0)

        layout.addWidget(self.view)

        self._build_scene()
        self.set_euler(0.0, 0.0, 0.0)

    def _build_scene(self) -> None:
        grid = gl.GLGridItem()
        grid.setSize(8, 8)
        grid.setSpacing(1, 1)
        self.view.addItem(grid)

        self._add_world_axis_lines()

        self.board = self._create_board_mesh()
        self.view.addItem(self.board)

        self.x_axis_body = self._make_line_item(color=(1.0, 0.15, 0.15, 1.0), width=4)
        self.y_axis_body = self._make_line_item(color=(0.2, 1.0, 0.35, 1.0), width=4)
        self.z_axis_body = self._make_line_item(color=(0.25, 0.55, 1.0, 1.0), width=4)

        self.view.addItem(self.x_axis_body)
        self.view.addItem(self.y_axis_body)
        self.view.addItem(self.z_axis_body)

    def _add_world_axis_lines(self) -> None:
        x = self._make_line_item(color=(1.0, 0.1, 0.1, 1.0), width=2)
        y = self._make_line_item(color=(0.1, 1.0, 0.1, 1.0), width=2)
        z = self._make_line_item(color=(0.2, 0.4, 1.0, 1.0), width=2)

        x.setData(pos=np.array([[-3.5, 0.0, 0.0], [3.5, 0.0, 0.0]], dtype=float))
        y.setData(pos=np.array([[0.0, -3.5, 0.0], [0.0, 3.5, 0.0]], dtype=float))
        z.setData(pos=np.array([[0.0, 0.0, -0.1], [0.0, 0.0, 3.5]], dtype=float))

        self.view.addItem(x)
        self.view.addItem(y)
        self.view.addItem(z)

    @staticmethod
    def _make_line_item(color, width: int) -> gl.GLLinePlotItem:
        return gl.GLLinePlotItem(
            pos=np.zeros((2, 3), dtype=float),
            color=color,
            width=width,
            antialias=True,
        )

    def _create_board_mesh(self) -> gl.GLMeshItem:
        # Thin cuboid model, roughly like a small controller board.
        lx = 3.2
        ly = 1.7
        lz = 0.16

        x = lx / 2.0
        y = ly / 2.0
        z = lz / 2.0

        verts = np.array(
            [
                [-x, -y, -z],
                [ x, -y, -z],
                [ x,  y, -z],
                [-x,  y, -z],
                [-x, -y,  z],
                [ x, -y,  z],
                [ x,  y,  z],
                [-x,  y,  z],
            ],
            dtype=float,
        )

        faces = np.array(
            [
                [0, 1, 2], [0, 2, 3],  # bottom
                [4, 6, 5], [4, 7, 6],  # top
                [0, 4, 5], [0, 5, 1],  # front
                [1, 5, 6], [1, 6, 2],  # right
                [2, 6, 7], [2, 7, 3],  # back
                [3, 7, 4], [3, 4, 0],  # left
            ],
            dtype=np.uint32,
        )

        colors = np.array(
            [
                [0.12, 0.30, 0.52, 1.0],
                [0.12, 0.30, 0.52, 1.0],
                [0.20, 0.72, 0.88, 1.0],
                [0.20, 0.72, 0.88, 1.0],
                [0.12, 0.52, 0.26, 1.0],
                [0.12, 0.52, 0.26, 1.0],
                [0.72, 0.40, 0.12, 1.0],
                [0.72, 0.40, 0.12, 1.0],
                [0.48, 0.20, 0.60, 1.0],
                [0.48, 0.20, 0.60, 1.0],
                [0.55, 0.55, 0.18, 1.0],
                [0.55, 0.55, 0.18, 1.0],
            ],
            dtype=float,
        )

        return gl.GLMeshItem(
            vertexes=verts,
            faces=faces,
            faceColors=colors,
            smooth=False,
            drawEdges=True,
            edgeColor=(1.0, 1.0, 1.0, 0.75),
        )

    def set_euler(self, roll_deg: float, pitch_deg: float, yaw_deg: float) -> None:
        self.roll_deg = float(roll_deg)
        self.pitch_deg = float(pitch_deg)
        self.yaw_deg = float(yaw_deg)

        self._apply_attitude_to_board()
        self._update_body_axes()

    def _signed_angles(self) -> tuple[float, float, float]:
        roll = self.roll_deg * self.ROLL_SIGN + self.ROLL_OFFSET_DEG
        pitch = self.pitch_deg * self.PITCH_SIGN + self.PITCH_OFFSET_DEG
        yaw = self.yaw_deg * self.YAW_SIGN + self.YAW_OFFSET_DEG
        return roll, pitch, yaw

    def _apply_attitude_to_board(self) -> None:
        roll, pitch, yaw = self._signed_angles()

        self.board.resetTransform()

        # ZYX order: yaw -> pitch -> roll.
        self.board.rotate(yaw, 0, 0, 1, local=False)
        self.board.rotate(pitch, 0, 1, 0, local=False)
        self.board.rotate(roll, 1, 0, 0, local=False)

    def _rotation_matrix(self) -> np.ndarray:
        roll_deg, pitch_deg, yaw_deg = self._signed_angles()

        roll = math.radians(roll_deg)
        pitch = math.radians(pitch_deg)
        yaw = math.radians(yaw_deg)

        cr, sr = math.cos(roll), math.sin(roll)
        cp, sp = math.cos(pitch), math.sin(pitch)
        cy, sy = math.cos(yaw), math.sin(yaw)

        rx = np.array(
            [
                [1.0, 0.0, 0.0],
                [0.0, cr, -sr],
                [0.0, sr, cr],
            ],
            dtype=float,
        )

        ry = np.array(
            [
                [cp, 0.0, sp],
                [0.0, 1.0, 0.0],
                [-sp, 0.0, cp],
            ],
            dtype=float,
        )

        rz = np.array(
            [
                [cy, -sy, 0.0],
                [sy, cy, 0.0],
                [0.0, 0.0, 1.0],
            ],
            dtype=float,
        )

        return rz @ ry @ rx

    def _update_body_axes(self) -> None:
        r = self._rotation_matrix()

        origin = np.array([0.0, 0.0, 0.22], dtype=float)
        axis_len = 2.4

        x_end = origin + r @ np.array([axis_len, 0.0, 0.0])
        y_end = origin + r @ np.array([0.0, axis_len, 0.0])
        z_end = origin + r @ np.array([0.0, 0.0, axis_len])

        self.x_axis_body.setData(pos=np.vstack([origin, x_end]))
        self.y_axis_body.setData(pos=np.vstack([origin, y_end]))
        self.z_axis_body.setData(pos=np.vstack([origin, z_end]))
