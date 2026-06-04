#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
proto_imu_test.py

Stage 4 IMU App protocol test tool.

This script tests MCU IMU commands over the existing UART reliable protocol.

Frame format used by current firmware:
    magic[2]   = A5 5A
    version    = 0x01
    type       = 0x01 REQ, 0x02 RESP, 0x03 NACK, 0x04 EVENT
    flags      = 0x00
    seq        = uint8
    cmd        = uint8
    len        = uint16 little-endian
    payload    = len bytes
    crc16      = uint16 little-endian, CRC16-CCITT-FALSE over version..payload

IMU command map matched to current command_service.h + imu_app.h:
    0x30 IMU_GET_RAW
    0x31 IMU_GET_FILTERED
    0x32 IMU_GET_STATUS
    0x36 IMU_GET_ALGO_STATS
    0x37 IMU_SET_SAMPLE_RATE
    0x38 IMU_START_STREAM
    0x39 IMU_STOP_STREAM
    0x3A IMU_START
    0x3B IMU_STOP
    0x3C IMU_GET_ATTITUDE
    0x3D IMU_CLEAR_STATS
"""

from __future__ import annotations

import argparse
import re
import sys
import time
from dataclasses import dataclass
from typing import Dict, Iterable, List, Optional, Tuple

try:
    import serial
except ImportError as exc:
    raise SystemExit("pyserial is required. Install with: pip install pyserial") from exc


PROTO_MAGIC = b"\xA5\x5A"
PROTO_VERSION = 0x01

FRAME_TYPE_REQ = 0x01
FRAME_TYPE_RESP = 0x02
FRAME_TYPE_NACK = 0x03
FRAME_TYPE_EVENT = 0x04

PROTO_ERROR_NAMES = {
    0x00: "OK",
    0x01: "CRC",
    0x02: "LEN",
    0x03: "UNKNOWN_CMD",
    0x04: "INVALID_STATE",
    0x05: "INVALID_PARAM",
    0x06: "BUSY",
    0x07: "INTERNAL_ERROR",
}

IMU_CMD_GET_RAW = 0x30
IMU_CMD_GET_FILTERED = 0x31
IMU_CMD_GET_STATUS = 0x32
IMU_CMD_GET_ALGO_STATS = 0x36
IMU_CMD_SET_SAMPLE_RATE = 0x37
IMU_CMD_START_STREAM = 0x38
IMU_CMD_STOP_STREAM = 0x39
IMU_CMD_START = 0x3A
IMU_CMD_STOP = 0x3B
IMU_CMD_GET_ATTITUDE = 0x3C
IMU_CMD_CLEAR_STATS = 0x3D

IMU_COMMANDS = {
    "raw": ("IMU_GET_RAW", IMU_CMD_GET_RAW),
    "filtered": ("IMU_GET_FILTERED", IMU_CMD_GET_FILTERED),
    "status": ("IMU_GET_STATUS", IMU_CMD_GET_STATUS),
    "stats": ("IMU_GET_ALGO_STATS", IMU_CMD_GET_ALGO_STATS),
    "set-period": ("IMU_SET_SAMPLE_RATE", IMU_CMD_SET_SAMPLE_RATE),
    "start-stream": ("IMU_START_STREAM", IMU_CMD_START_STREAM),
    "stop-stream": ("IMU_STOP_STREAM", IMU_CMD_STOP_STREAM),
    "start": ("IMU_START", IMU_CMD_START),
    "stop": ("IMU_STOP", IMU_CMD_STOP),
    "attitude": ("IMU_GET_ATTITUDE", IMU_CMD_GET_ATTITUDE),
    "clear": ("IMU_CLEAR_STATS", IMU_CMD_CLEAR_STATS),
}

CMD_NAME_BY_ID = {cmd: name for name, cmd in (v for v in IMU_COMMANDS.values())}

EXPECTED_KEYS = {
    IMU_CMD_GET_RAW: ["ax=", "ay=", "az=", "gx=", "gy=", "gz=", "temp=", "tick="],
    IMU_CMD_GET_FILTERED: ["ax_mg=", "ay_mg=", "az_mg=", "gx_mdps=", "gy_mdps=", "gz_mdps=", "tick="],
    IMU_CMD_GET_STATUS: ["state=", "started=", "stream=", "err=", "sample=", "period="],
    IMU_CMD_GET_ALGO_STATS: ["run=", "sample=", "read_err=", "att=", "att_err=", "evt=", "drop="],
    IMU_CMD_SET_SAMPLE_RATE: ["period=", "event_period="],
    IMU_CMD_START_STREAM: ["stream=1"],
    IMU_CMD_STOP_STREAM: ["stream=0"],
    IMU_CMD_START: ["started=1"],
    IMU_CMD_STOP: ["started=0"],
    IMU_CMD_GET_ATTITUDE: ["roll_cdeg=", "pitch_cdeg=", "yaw_cdeg=", "q0_m=", "q1_m=", "q2_m=", "q3_m=", "tick="],
    IMU_CMD_CLEAR_STATS: ["cleared=1"],
}


@dataclass
class ProtoFrame:
    frame_type: int
    flags: int
    seq: int
    cmd: int
    payload: bytes

    @property
    def payload_ascii(self) -> str:
        return self.payload.decode("utf-8", errors="replace")

    @property
    def type_name(self) -> str:
        return {
            FRAME_TYPE_REQ: "REQ",
            FRAME_TYPE_RESP: "RESP",
            FRAME_TYPE_NACK: "NACK",
            FRAME_TYPE_EVENT: "EVENT",
        }.get(self.frame_type, f"0x{self.frame_type:02X}")

    def summary(self) -> str:
        text = self.payload_ascii
        hex_payload = " ".join(f"{b:02X}" for b in self.payload)
        return (
            f"type=0x{self.frame_type:02X}({self.type_name}), flags=0x{self.flags:02X}, "
            f"seq={self.seq}, cmd=0x{self.cmd:02X}, len={len(self.payload)}, "
            f"payload_ascii=[{text}], payload_hex=[{hex_payload}]"
        )


class CrcError(Exception):
    pass


class TimeoutError(Exception):
    pass


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc & 0xFFFF


def build_frame(seq: int, cmd: int, payload: bytes = b"", flags: int = 0) -> bytes:
    if len(payload) > 0xFFFF:
        raise ValueError("payload too large")

    body = bytearray()
    body.append(PROTO_VERSION)
    body.append(FRAME_TYPE_REQ)
    body.append(flags & 0xFF)
    body.append(seq & 0xFF)
    body.append(cmd & 0xFF)
    body.extend(len(payload).to_bytes(2, "little"))
    body.extend(payload)

    crc = crc16_ccitt_false(bytes(body))

    frame = bytearray(PROTO_MAGIC)
    frame.extend(body)
    frame.extend(crc.to_bytes(2, "little"))
    return bytes(frame)


class ProtoParser:
    def __init__(self) -> None:
        self.buf = bytearray()

    def feed(self, data: bytes) -> List[ProtoFrame]:
        self.buf.extend(data)
        frames: List[ProtoFrame] = []

        while True:
            magic_pos = self.buf.find(PROTO_MAGIC)
            if magic_pos < 0:
                if len(self.buf) > 1:
                    del self.buf[:-1]
                break

            if magic_pos > 0:
                del self.buf[:magic_pos]

            # Minimum frame: magic 2 + version 1 + type 1 + flags 1 + seq 1 + cmd 1 + len 2 + crc 2 = 11
            if len(self.buf) < 11:
                break

            version = self.buf[2]
            if version != PROTO_VERSION:
                del self.buf[0]
                continue

            payload_len = int.from_bytes(self.buf[7:9], "little")
            total_len = 2 + 7 + payload_len + 2

            if len(self.buf) < total_len:
                break

            raw = bytes(self.buf[:total_len])
            body = raw[2:-2]
            rx_crc = int.from_bytes(raw[-2:], "little")
            calc_crc = crc16_ccitt_false(body)

            if rx_crc != calc_crc:
                # Drop one byte and resync instead of killing the whole stream.
                del self.buf[0]
                continue

            frame = ProtoFrame(
                frame_type=raw[3],
                flags=raw[4],
                seq=raw[5],
                cmd=raw[6],
                payload=raw[9:-2],
            )
            frames.append(frame)
            del self.buf[:total_len]

        return frames


class ImuProtoClient:
    def __init__(self, port: str, baud: int, timeout: float, verbose: bool = False) -> None:
        self.ser = serial.Serial(port=port, baudrate=baud, timeout=0.02)
        self.timeout = timeout
        self.verbose = verbose
        self.seq = 1
        self.parser = ProtoParser()

    def close(self) -> None:
        self.ser.close()

    def next_seq(self) -> int:
        seq = self.seq
        self.seq = (self.seq + 1) & 0xFF
        if self.seq == 0:
            self.seq = 1
        return seq

    def drain(self, duration_s: float = 0.1) -> List[ProtoFrame]:
        end = time.time() + duration_s
        frames: List[ProtoFrame] = []
        while time.time() < end:
            n = self.ser.in_waiting
            if n:
                frames.extend(self.parser.feed(self.ser.read(n)))
            time.sleep(0.005)
        return frames

    def request(self, cmd: int, payload: bytes = b"", name: Optional[str] = None) -> ProtoFrame:
        seq = self.next_seq()
        frame = build_frame(seq, cmd, payload)
        cmd_name = name or CMD_NAME_BY_ID.get(cmd, f"CMD_0x{cmd:02X}")

        if self.verbose:
            print(f"[TX] {cmd_name}: seq={seq}, cmd=0x{cmd:02X}, bytes={frame.hex(' ').upper()}")

        self.ser.write(frame)
        self.ser.flush()

        deadline = time.time() + self.timeout

        while time.time() < deadline:
            n = self.ser.in_waiting
            if n:
                frames = self.parser.feed(self.ser.read(n))
                for rx in frames:
                    if rx.frame_type == FRAME_TYPE_EVENT:
                        print_event(rx)
                        continue

                    if rx.seq == seq and rx.cmd == cmd:
                        if rx.frame_type == FRAME_TYPE_NACK:
                            err = rx.payload[0] if rx.payload else 0xFF
                            err_name = PROTO_ERROR_NAMES.get(err, f"0x{err:02X}")
                            raise RuntimeError(
                                f"NACK for {cmd_name}: seq={seq}, cmd=0x{cmd:02X}, "
                                f"err=0x{err:02X}({err_name}), payload={rx.payload.hex(' ').upper()}"
                            )

                        return rx

            time.sleep(0.002)

        raise TimeoutError(f"timeout waiting for {cmd_name}, seq={seq}, cmd=0x{cmd:02X}")


def parse_key_values(text: str) -> Dict[str, str]:
    result: Dict[str, str] = {}
    for part in text.split(","):
        part = part.strip()
        if "=" in part:
            k, v = part.split("=", 1)
            result[k.strip()] = v.strip()
    return result


def as_int(value: str, default: int = 0) -> int:
    try:
        return int(value, 0)
    except Exception:
        return default


def check_expected_payload(cmd: int, text: str) -> Tuple[bool, List[str]]:
    missing = []
    for key in EXPECTED_KEYS.get(cmd, []):
        if key not in text:
            missing.append(key)
    return len(missing) == 0, missing


def print_event(frame: ProtoFrame) -> None:
    text = frame.payload_ascii
    print(f"[EVENT] cmd=0x{frame.cmd:02X}, seq={frame.seq}, payload=[{text}]")


def print_resp(label: str, frame: ProtoFrame) -> bool:
    text = frame.payload_ascii
    ok, missing = check_expected_payload(frame.cmd, text)
    status = "PASS" if ok else "WARN"
    print(f"[{status}] {label} - {frame.summary()}")
    if missing:
        print(f"       missing keys: {missing}")
    return ok


def run_resp_test(client: ImuProtoClient, label: str, cmd: int, payload: bytes = b"") -> bool:
    try:
        frame = client.request(cmd, payload, label)
        return print_resp(label, frame)
    except Exception as exc:
        print(f"[FAIL] {label} - {exc}")
        return False


def run_basic_sequence(client: ImuProtoClient, sample_period_ms: Optional[int] = None) -> bool:
    tests: List[Tuple[str, int, bytes]] = [
        ("imu_get_status_before_start", IMU_CMD_GET_STATUS, b""),
        ("imu_start", IMU_CMD_START, b""),
        ("imu_get_status_after_start", IMU_CMD_GET_STATUS, b""),
    ]

    if sample_period_ms is not None:
        tests.append(("imu_set_sample_period", IMU_CMD_SET_SAMPLE_RATE, str(sample_period_ms).encode("ascii")))

    # Give MCU several ImuApp_Run cycles to collect samples.
    passed = 0
    total = 0

    for label, cmd, payload in tests:
        total += 1
        if run_resp_test(client, label, cmd, payload):
            passed += 1
        time.sleep(0.05)

    time.sleep(0.25)

    more_tests: List[Tuple[str, int, bytes]] = [
        ("imu_get_raw", IMU_CMD_GET_RAW, b""),
        ("imu_get_filtered", IMU_CMD_GET_FILTERED, b""),
        ("imu_get_attitude", IMU_CMD_GET_ATTITUDE, b""),
        ("imu_get_algo_stats", IMU_CMD_GET_ALGO_STATS, b""),
        ("imu_start_stream", IMU_CMD_START_STREAM, b""),
    ]

    for label, cmd, payload in more_tests:
        total += 1
        if run_resp_test(client, label, cmd, payload):
            passed += 1
        time.sleep(0.05)

    print("[INFO] listening for IMU EVENT frames for 1.0s ...")
    events = client.drain(1.0)
    event_count = 0
    attitude_event_count = 0
    for ev in events:
        if ev.frame_type == FRAME_TYPE_EVENT:
            event_count += 1
            if b"att," in ev.payload:
                attitude_event_count += 1
            print_event(ev)

    print(f"[INFO] event_count={event_count}, attitude_event_count={attitude_event_count}")

    final_tests: List[Tuple[str, int, bytes]] = [
        ("imu_stop_stream", IMU_CMD_STOP_STREAM, b""),
        ("imu_stop", IMU_CMD_STOP, b""),
        ("imu_get_status_after_stop", IMU_CMD_GET_STATUS, b""),
    ]

    for label, cmd, payload in final_tests:
        total += 1
        if run_resp_test(client, label, cmd, payload):
            passed += 1
        time.sleep(0.05)

    print(f"[SUMMARY] basic_sequence: PASS {passed}/{total}")
    return passed == total


def run_monitor(client: ImuProtoClient, duration_s: float, interval_s: float) -> None:
    print("[INFO] starting IMU monitor")
    print("[INFO] press Ctrl+C to stop")

    end_time = time.time() + duration_s if duration_s > 0 else None

    run_resp_test(client, "imu_start", IMU_CMD_START)
    time.sleep(0.1)

    try:
        while True:
            if end_time is not None and time.time() >= end_time:
                break

            raw_frame = client.request(IMU_CMD_GET_RAW, name="IMU_GET_RAW")
            att_frame = client.request(IMU_CMD_GET_ATTITUDE, name="IMU_GET_ATTITUDE")
            stats_frame = client.request(IMU_CMD_GET_ALGO_STATS, name="IMU_GET_ALGO_STATS")

            raw = parse_key_values(raw_frame.payload_ascii)
            att = parse_key_values(att_frame.payload_ascii)
            stats = parse_key_values(stats_frame.payload_ascii)

            roll = as_int(att.get("roll_cdeg", "0")) / 100.0
            pitch = as_int(att.get("pitch_cdeg", "0")) / 100.0
            yaw = as_int(att.get("yaw_cdeg", "0")) / 100.0

            print(
                "[MON] "
                f"ax={raw.get('ax','?')} ay={raw.get('ay','?')} az={raw.get('az','?')} "
                f"gx={raw.get('gx','?')} gy={raw.get('gy','?')} gz={raw.get('gz','?')} | "
                f"roll={roll:.2f} pitch={pitch:.2f} yaw={yaw:.2f} | "
                f"sample={stats.get('sample','?')} att={stats.get('att','?')} "
                f"read_us={stats.get('read_us','?')} algo_us={stats.get('algo_us','?')}"
            )

            # Also print async event if any arrived.
            for ev in client.drain(0.01):
                if ev.frame_type == FRAME_TYPE_EVENT:
                    print_event(ev)

            time.sleep(interval_s)
    finally:
        run_resp_test(client, "imu_stop", IMU_CMD_STOP)


def run_stream_test(client: ImuProtoClient, duration_s: float, sample_period_ms: Optional[int]) -> bool:
    passed = True

    if not run_resp_test(client, "imu_start", IMU_CMD_START):
        passed = False

    if sample_period_ms is not None:
        if not run_resp_test(client, "imu_set_sample_period", IMU_CMD_SET_SAMPLE_RATE, str(sample_period_ms).encode("ascii")):
            passed = False

    if not run_resp_test(client, "imu_start_stream", IMU_CMD_START_STREAM):
        passed = False

    print(f"[INFO] collecting stream events for {duration_s:.1f}s ...")
    start = time.time()
    event_count = 0
    attitude_count = 0

    try:
        while time.time() - start < duration_s:
            frames = client.drain(0.1)
            for frame in frames:
                if frame.frame_type == FRAME_TYPE_EVENT:
                    event_count += 1
                    if b"att," in frame.payload:
                        attitude_count += 1
                    print_event(frame)
    finally:
        if not run_resp_test(client, "imu_stop_stream", IMU_CMD_STOP_STREAM):
            passed = False
        if not run_resp_test(client, "imu_stop", IMU_CMD_STOP):
            passed = False

    print(f"[SUMMARY] stream_test: event_count={event_count}, attitude_count={attitude_count}")
    if attitude_count == 0:
        print("[WARN] no attitude events received. Check McuInfoApp_Run(), ProtocolManager_Process(), and event queue.")
        passed = False

    return passed


def run_single_command(client: ImuProtoClient, command_name: str, payload: bytes) -> bool:
    if command_name not in IMU_COMMANDS:
        raise SystemExit(f"unknown command {command_name}. Available: {', '.join(IMU_COMMANDS)}")
    label, cmd = IMU_COMMANDS[command_name]
    return run_resp_test(client, label, cmd, payload)


def list_commands() -> None:
    print("Available IMU commands:")
    for key, (label, cmd) in IMU_COMMANDS.items():
        print(f"  {key:14s} 0x{cmd:02X} {label}")


def main(argv: Optional[Iterable[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Stage 4 IMU App UART protocol test")
    parser.add_argument("--port", required=True, help="serial port, e.g. COM19 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--list", action="store_true", help="list supported commands and exit")

    sub = parser.add_subparsers(dest="mode", required=False)

    sub.add_parser("basic", help="run full basic IMU command sequence")

    cmd_parser = sub.add_parser("cmd", help="send one IMU command")
    cmd_parser.add_argument("name", choices=sorted(IMU_COMMANDS.keys()))
    cmd_parser.add_argument("--payload", default="", help="ASCII payload, e.g. 10 for set-period")

    monitor_parser = sub.add_parser("monitor", help="periodically query raw + attitude + stats")
    monitor_parser.add_argument("--duration", type=float, default=10.0, help="seconds; <=0 means forever")
    monitor_parser.add_argument("--interval", type=float, default=0.5, help="query interval seconds")

    stream_parser = sub.add_parser("stream", help="start event stream and listen for events")
    stream_parser.add_argument("--duration", type=float, default=5.0)
    stream_parser.add_argument("--period-ms", type=int, default=None, help="optional IMU sample period payload for 0x37")

    parser.add_argument("--period-ms", type=int, default=None, help="optional sample period for basic mode")

    args = parser.parse_args(list(argv) if argv is not None else None)

    if args.list:
        list_commands()
        return 0

    mode = args.mode or "basic"

    client = ImuProtoClient(args.port, args.baud, args.timeout, args.verbose)

    try:
        # Clear stale bytes/events after opening port.
        client.drain(0.2)

        if mode == "basic":
            ok = run_basic_sequence(client, sample_period_ms=args.period_ms)
        elif mode == "cmd":
            ok = run_single_command(client, args.name, args.payload.encode("ascii"))
        elif mode == "monitor":
            run_monitor(client, duration_s=args.duration, interval_s=args.interval)
            ok = True
        elif mode == "stream":
            ok = run_stream_test(client, duration_s=args.duration, sample_period_ms=args.period_ms)
        else:
            raise SystemExit(f"unsupported mode: {mode}")

        return 0 if ok else 1

    finally:
        client.close()


if __name__ == "__main__":
    raise SystemExit(main())
