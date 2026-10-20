# telemetry_formatter.py: offline pack/unpack test for the telemetry packet
import struct


def pack_telemetry(rms: float, interval: int, led: str) -> bytes:
    """Pack fields into the 8-byte '<BfHB' packet used by the firmware."""
    return struct.pack('<BfHB', 0xAA, rms, interval, 1 if led.strip().upper() == "ON" else 0)


if __name__ == '__main__':
    pkt = pack_telemetry(0.0421, 150, "ON")
    assert len(pkt) == 8
    print(f"Length: {len(pkt)} bytes")
    print(f"Hex: {pkt.hex(' ')}")
    header, rms, interval, led = struct.unpack('<BfHB', pkt)
    print(f"Decoded -> header=0x{header:02X}, rms={rms:.4f}, interval={interval}, led={led}")
