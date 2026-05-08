"""
Generate firmware sample files for MotorDev FW-flash UI testing.

Outputs into the same directory:
  fw_sample_simple.bin       — 256 bytes, value = index (0x00..0xFF)
  fw_sample_simple.hex       — same 256 bytes encoded as Intel HEX (single segment)
  fw_sample_multiseg.hex     — 3 separated segments to exercise .hex segment merging
"""

from pathlib import Path
import zlib

OUT_DIR = Path(__file__).parent


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def hex_record(addr: int, type_: int, data: bytes) -> str:
    bc = len(data)
    hi = (addr >> 8) & 0xFF
    lo = addr & 0xFF
    s = bc + hi + lo + type_ + sum(data)
    csum = (-s) & 0xFF
    body = bytes([bc, hi, lo, type_]) + data + bytes([csum])
    return ":" + body.hex().upper()


def write_bin_simple():
    data = bytes(range(256))  # 0x00..0xFF
    path = OUT_DIR / "fw_sample_simple.bin"
    path.write_bytes(data)
    return path, data


def write_hex_simple(data: bytes):
    """Encode the same bytes as Intel HEX, 16 bytes per record, starting at 0x0000."""
    lines = []
    chunk = 16
    for off in range(0, len(data), chunk):
        lines.append(hex_record(off, 0x00, data[off:off + chunk]))
    lines.append(":00000001FF")  # EOF
    path = OUT_DIR / "fw_sample_simple.hex"
    path.write_text("\n".join(lines) + "\n")
    return path


def write_hex_multiseg():
    """3 disjoint segments. Parser must merge with 0xFF padding between them."""
    seg1_addr, seg1_data = 0x0000, bytes([0xAA] * 16)
    seg2_addr, seg2_data = 0x0100, bytes([0xBB] * 16)
    seg3_addr, seg3_data = 0x0200, bytes([0xCC] * 16)

    lines = []
    for addr, payload in [(seg1_addr, seg1_data),
                          (seg2_addr, seg2_data),
                          (seg3_addr, seg3_data)]:
        lines.append(hex_record(addr, 0x00, payload))
    lines.append(":00000001FF")
    path = OUT_DIR / "fw_sample_multiseg.hex"
    path.write_text("\n".join(lines) + "\n")

    # Compute the merged image so we can echo expected stats for sanity check.
    lo = min(seg1_addr, seg2_addr, seg3_addr)
    hi_byte = max(seg3_addr + len(seg3_data) - 1,
                  seg2_addr + len(seg2_data) - 1,
                  seg1_addr + len(seg1_data) - 1)
    merged = bytearray([0xFF] * (hi_byte - lo + 1))
    for addr, payload in [(seg1_addr, seg1_data),
                          (seg2_addr, seg2_data),
                          (seg3_addr, seg3_data)]:
        for i, b in enumerate(payload):
            merged[addr - lo + i] = b
    return path, merged, lo, hi_byte


def main():
    bin_path, bin_bytes = write_bin_simple()
    hex_path = write_hex_simple(bin_bytes)
    multi_path, multi_merged, lo, hi_byte = write_hex_multiseg()

    print(f"{bin_path.name}:")
    print(f"  size       = {len(bin_bytes)} bytes")
    print(f"  CRC32      = 0x{crc32(bin_bytes):08X}")
    print()
    print(f"{hex_path.name}:")
    print(f"  same 256 bytes as .bin, single segment 0x0000-0x00FF")
    print(f"  CRC32 of merged data = 0x{crc32(bin_bytes):08X}  (must match .bin)")
    print()
    print(f"{multi_path.name}:")
    print(f"  segments   = 3 (0x0000, 0x0100, 0x0200, each 16 bytes)")
    print(f"  merged     = {len(multi_merged)} bytes (0x{lo:04X} - 0x{hi_byte:04X}, padded with 0xFF)")
    print(f"  effective  = 48 bytes")
    print(f"  CRC32      = 0x{crc32(bytes(multi_merged)):08X}")


if __name__ == "__main__":
    main()
