from __future__ import annotations

import os
import random


AVAILABLE_PATTERNS = [
    "zero",
    "ff",
    "counter",
    "random",
    "urandom",
    "ascii",
    "sof_noise",
]


def make_pattern(pattern: str, size: int, seed: int = 1) -> bytes:
    if size < 0:
        raise ValueError("size must be >= 0")

    pattern = pattern.lower()

    if pattern == "zero":
        return bytes([0x00]) * size

    if pattern == "ff":
        return bytes([0xFF]) * size

    if pattern == "counter":
        return bytes((i & 0xFF) for i in range(size))

    if pattern == "random":
        rng = random.Random(seed)
        return bytes(rng.randrange(0, 256) for _ in range(size))

    if pattern == "urandom":
        return os.urandom(size)

    if pattern == "ascii":
        base = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789\r\n"
        return bytes(base[i % len(base)] for i in range(size))

    if pattern == "sof_noise":
        base = bytes([0xA5, 0x5A, 0x00, 0xFF, 0x11, 0x22, 0xA5, 0x33])
        return bytes(base[i % len(base)] for i in range(size))

    raise ValueError(f"unknown pattern: {pattern}")