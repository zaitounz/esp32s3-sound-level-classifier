# sound_led_monitor.py: live decoder for the 8-byte telemetry packets
import struct
import sys
import time

import serial

PORT, BAUD = '/dev/cu.usbmodem3101', 115200   # change to your board's serial port
PACKET_SIZE = 8
HEADER_SYNC = 0xAA
VALID_INTERVALS = {0, 40, 150, 500}


def classify_interval(interval: int) -> tuple[str, str]:
    """Return the blink-rate description and sound level for an interval."""
    if interval == 0:
        return "OFF (0 ms)", "Level 1: Background"
    if interval >= 400:
        return f"Slow ({interval} ms)", "Level 2: Normal speech"
    if interval >= 100:
        return f"Fast ({interval} ms)", "Level 3: Loud speech"
    return f"Strobe ({interval} ms)", "Level 4: Shout / clap"


def run_monitor():
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
        print(f"Connected to {PORT} at {BAUD} baud.\n")
    except Exception as e:
        print(f"Error opening {PORT}: {e}\nClose the Arduino Serial Monitor first.")
        sys.exit(1)

    time.sleep(1.5)            # let the connection settle
    ser.reset_input_buffer()   # drop partial packets / boot text

    print(f"{'RMS':<10} | {'Blink Rate':<18} | {'LED':<5} | Level")
    print("-" * 60)
    try:
        while True:
            b = ser.read(1)                       # 1. hunt for sync byte
            if not b or b[0] != HEADER_SYNC:
                continue
            rest = ser.read(PACKET_SIZE - 1)      # 2. read remaining 7 bytes
            if len(rest) != PACKET_SIZE - 1:
                continue
            _, rms, interval, led = struct.unpack('<BfHB', b + rest)

            # 0xAA can appear inside the float, so reject misaligned frames
            if led not in (0, 1) or interval not in VALID_INTERVALS or not (0.0 <= rms <= 1.5):
                continue

            rate, level = classify_interval(interval)
            print(f"{rms:<10.4f} | {rate:<18} | {'ON' if led else 'OFF':<5} | {level}")
    except KeyboardInterrupt:
        print("\nStopping monitor...")
    finally:
        ser.close()


if __name__ == '__main__':
    run_monitor()
