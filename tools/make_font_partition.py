#!/usr/bin/env python3
from __future__ import annotations

import argparse
import struct
from pathlib import Path

MAGIC = b"SUFT"
HEADER_SIZE = 4096


def main() -> int:
    parser = argparse.ArgumentParser(description="Pack a raw font partition image")
    parser.add_argument("input_font")
    parser.add_argument("output_image")
    parser.add_argument("partition_size", type=lambda x: int(x, 0))
    args = parser.parse_args()

    input_path = Path(args.input_font)
    output_path = Path(args.output_image)
    font_data = input_path.read_bytes()

    if len(font_data) + HEADER_SIZE > args.partition_size:
        raise SystemExit(
            f"font size {len(font_data)} + header {HEADER_SIZE} exceeds partition size {args.partition_size}"
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("wb") as f:
        header = bytearray(HEADER_SIZE)
        header[:4] = MAGIC
        header[4:8] = struct.pack("<I", len(font_data))
        f.write(header)
        f.write(font_data)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
