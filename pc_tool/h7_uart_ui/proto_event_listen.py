from __future__ import annotations

import argparse
import time
from dataclasses import dataclass

import serial


SOF1 = 0xA5
SOF2 = 0x5A
VER = 0x01

TYPE_REQ = 0x01
TYPE_RESP = 0x02
TYPE_ACK = 0x03
TYPE_NACK = 0x04
TYPE_EVENT = 0x05
TYPE_DATA = 0x06
TYPE_WINDOW_ACK = 0x07

MAX_PAYLOAD = 128


MCU_INFO_EVENT_BOOT = 0x81
MCU_INFO_EVENT_HEARTBEAT = 0x82
MCU_INFO_EVENT_RUNTIME_STATUS = 0x83
MCU_INFO_EVENT_UART_WARNING = 0x84
MCU_INFO_EVENT_APP_MESSAGE = 0x85
MCU_INFO_EVENT_FAULT = 0x86


EVENT_NAME_MAP = {
    MCU_INFO_EVENT_BOOT: "BOOT",
    MCU_INFO_EVENT_HEARTBEAT: "HEARTBEAT",
    MCU_INFO_EVENT_RUNTIME_STATUS: "RUNTIME_STATUS",
    MCU_INFO_EVENT_UART_WARNING: "UART_WARNING",
    MCU_INFO_EVENT_APP_MESSAGE: "APP_MESSAGE",
    MCU_INFO_EVENT_FAULT: "FAULT",
}


APP_ID_MAP = {
    0: "SYSTEM",
    1: "UART",
    2: "FAULT",
    3: "USER",
}


@dataclass
class ProtoFrame:
    frame_type: int
    flags: int
    seq: int
    cmd: int
    payload: bytes


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF

    for b in data:
        crc ^= b << 8

        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF

    return crc


def try_parse_one_frame(buf: bytearray) -> ProtoFrame | None:
    """
    Parse one protocol frame from mixed UART stream.

    MCU UART may also print ASCII logs, so this parser skips all bytes
    before SOF1/SOF2.
    """
    while len(buf) >= 2:
        if buf[0] == SOF1 and buf[1] == SOF2:
            break
        del buf[0]

    if len(buf) < 11:
        return None

    ver = buf[2]
    frame_type = buf[3]
    flags = buf[4]
    seq = buf[5]
    cmd = buf[6]
    length = buf[7] | (buf[8] << 8)

    if ver != VER:
        del buf[0]
        return None

    if length > MAX_PAYLOAD:
        del buf[0]
        return None

    total_len = 2 + 7 + length + 2

    if len(buf) < total_len:
        return None

    frame_bytes = bytes(buf[:total_len])
    del buf[:total_len]

    recv_crc = frame_bytes[-2] | (frame_bytes[-1] << 8)
    calc_crc = crc16_ccitt_false(frame_bytes[2:-2])

    if recv_crc != calc_crc:
        print(f"[PC] CRC error: recv=0x{recv_crc:04X}, calc=0x{calc_crc:04X}")
        return None

    payload = frame_bytes[9:-2]

    return ProtoFrame(
        frame_type=frame_type,
        flags=flags,
        seq=seq,
        cmd=cmd,
        payload=payload,
    )


def decode_event_payload(payload: bytes) -> str:
    """
    Current EVENT payload format:

        byte0      app_id
        byte1      original_event_id
        byte2~5    tick_ms little-endian
        byte6..N   event payload
    """
    if len(payload) < 6:
        return f"invalid_event_payload_len={len(payload)}, raw={payload.hex(' ').upper()}"

    app_id = payload[0]
    original_event_id = payload[1]
    tick_ms = (
        payload[2]
        | (payload[3] << 8)
        | (payload[4] << 16)
        | (payload[5] << 24)
    )

    data = payload[6:]

    app_name = APP_ID_MAP.get(app_id, f"APP_{app_id}")
    event_name = EVENT_NAME_MAP.get(original_event_id, f"EVENT_0x{original_event_id:02X}")

    data_hex = data.hex(" ").upper()

    try:
        data_ascii = data.decode("utf-8", errors="replace")
    except Exception:
        data_ascii = ""

    return (
        f"app_id={app_id}({app_name}), "
        f"event_id=0x{original_event_id:02X}({event_name}), "
        f"tick={tick_ms} ms, "
        f"data_hex=[{data_hex}], "
        f"data_ascii=[{data_ascii}]"
    )


def frame_type_name(frame_type: int) -> str:
    names = {
        TYPE_REQ: "REQ",
        TYPE_RESP: "RESP",
        TYPE_ACK: "ACK",
        TYPE_NACK: "NACK",
        TYPE_EVENT: "EVENT",
        TYPE_DATA: "DATA",
        TYPE_WINDOW_ACK: "WINDOW_ACK",
    }

    return names.get(frame_type, f"UNKNOWN_0x{frame_type:02X}")


def event_name(event_id: int) -> str:
    return EVENT_NAME_MAP.get(event_id, f"EVENT_0x{event_id:02X}")


def listen_events(
    port: str,
    baud: int,
    timeout_s: float,
    max_events: int,
    show_all_frames: bool,
) -> int:
    rx_buf = bytearray()
    event_count = 0
    start_time = time.time()

    print(f"[PC] Open {port} @ {baud}")
    print("[PC] Listening for EVENT frames...")
    print("[PC] Press Ctrl+C to stop.")
    print("-" * 80)

    with serial.Serial(port=port, baudrate=baud, timeout=0.02, write_timeout=0.5) as ser:
        time.sleep(0.2)

        try:
            ser.reset_input_buffer()
            ser.reset_output_buffer()
        except Exception:
            pass

        while True:
            if timeout_s > 0 and (time.time() - start_time) > timeout_s:
                print("[PC] Listen timeout")
                break

            n = ser.in_waiting

            if n > 0:
                data = ser.read(n)
                rx_buf.extend(data)

                while True:
                    frame = try_parse_one_frame(rx_buf)

                    if frame is None:
                        break

                    type_name = frame_type_name(frame.frame_type)

                    if frame.frame_type == TYPE_EVENT:
                        event_count += 1

                        print(
                            f"[EVENT] seq={frame.seq}, "
                            f"cmd=0x{frame.cmd:02X}({event_name(frame.cmd)}), "
                            f"len={len(frame.payload)}"
                        )
                        print(f"[EVENT] {decode_event_payload(frame.payload)}")
                        print("-" * 80)

                        if max_events > 0 and event_count >= max_events:
                            print(f"[PC] Reached max events: {max_events}")
                            return 0

                    elif show_all_frames:
                        payload_hex = frame.payload.hex(" ").upper()

                        try:
                            payload_ascii = frame.payload.decode("utf-8", errors="replace")
                        except Exception:
                            payload_ascii = ""

                        print(
                            f"[FRAME] type={type_name}, "
                            f"seq={frame.seq}, "
                            f"cmd=0x{frame.cmd:02X}, "
                            f"len={len(frame.payload)}, "
                            f"payload_hex=[{payload_hex}], "
                            f"payload_ascii=[{payload_ascii}]"
                        )
                        print("-" * 80)

            time.sleep(0.003)

    print(f"[SUMMARY] event_count={event_count}")

    return 0 if event_count > 0 else 1


def main() -> None:
    parser = argparse.ArgumentParser(description="Listen for DM-MC02 H723 UART EVENT frames")
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM19")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=10.0, help="Listen timeout in seconds, 0 means forever")
    parser.add_argument("--max-events", type=int, default=1, help="Stop after receiving this many events, 0 means unlimited")
    parser.add_argument("--show-all-frames", action="store_true", help="Print non-EVENT frames too")

    args = parser.parse_args()

    raise SystemExit(
        listen_events(
            port=args.port,
            baud=args.baud,
            timeout_s=args.timeout,
            max_events=args.max_events,
            show_all_frames=args.show_all_frames,
        )
    )


if __name__ == "__main__":
    main()