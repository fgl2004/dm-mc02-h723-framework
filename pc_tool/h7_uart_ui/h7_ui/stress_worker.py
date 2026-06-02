from __future__ import annotations

import time

from PySide6.QtCore import QObject, Signal, Slot

from .patterns import make_pattern


class StressWorker(QObject):
    progress = Signal(int, int)
    finished = Signal(int, float)
    error = Signal(str)

    def __init__(self) -> None:
        super().__init__()
        self._stop_requested = False
        self._serial_worker = None

    def set_serial_worker(self, serial_worker) -> None:
        self._serial_worker = serial_worker

    def request_stop_direct(self) -> None:
        """
        This function is called directly by UI thread.

        Do not rely on queued signal here, because run_stress() occupies
        the stress thread and queued stop events may not be processed in time.
        """
        self._stop_requested = True

    @Slot(str, int, int, float, int)
    def run_stress(self, pattern: str, size: int, chunk: int, interval_ms: float, seed: int) -> None:
        self._stop_requested = False

        if self._serial_worker is None:
            self.error.emit("serial worker is not attached")
            return

        if size < 0:
            self.error.emit("size must be >= 0")
            return

        if chunk <= 0:
            self.error.emit("chunk must be > 0")
            return

        try:
            data = make_pattern(pattern, size, seed)
        except Exception as exc:
            self.error.emit(str(exc))
            return

        sent = 0
        start_time = time.time()

        try:
            while sent < len(data):
                if self._stop_requested:
                    break

                end = min(sent + chunk, len(data))
                part = data[sent:end]

                written = self._serial_worker.write_bytes(part)

                sent += written
                self.progress.emit(sent, len(data))

                if interval_ms > 0:
                    sleep_end = time.time() + interval_ms / 1000.0
                    while time.time() < sleep_end:
                        if self._stop_requested:
                            break
                        time.sleep(0.001)

            elapsed = time.time() - start_time
            self.finished.emit(sent, elapsed)

        except Exception as exc:
            self.error.emit(f"stress failed: {exc}")