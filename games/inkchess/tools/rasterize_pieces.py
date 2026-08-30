#!/usr/bin/env python3
"""Rasterize GNOME Chess 'simple' SVG pieces for InkChess.

Output format is intentionally tiny and dependency-free at runtime:
for each 128x128 piece, write 16,384 Gray8 bytes followed by
16,384 straight-alpha bytes. SVG rendering happens only at build time.
"""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import tempfile

from PIL import Image

SIZE = 128
PIECES = (
    "whitePawn",
    "whiteKnight",
    "whiteBishop",
    "whiteRook",
    "whiteQueen",
    "whiteKing",
    "blackPawn",
    "blackKnight",
    "blackBishop",
    "blackRook",
    "blackQueen",
    "blackKing",
)


def rasterize(src: pathlib.Path, dst: pathlib.Path) -> None:
    if shutil.which("rsvg-convert") is None:
        raise SystemExit("rsvg-convert is required (package: librsvg2-bin)")

    with tempfile.NamedTemporaryFile(suffix=".png") as tmp:
        subprocess.run(
            [
                "rsvg-convert",
                "--format=png",
                f"--width={SIZE}",
                f"--height={SIZE}",
                "--keep-aspect-ratio",
                "--output",
                tmp.name,
                str(src),
            ],
            check=True,
        )
        image = Image.open(tmp.name).convert("RGBA")

    if image.size != (SIZE, SIZE):
        raise SystemExit(f"unexpected raster size for {src}: {image.size}")

    gray = bytearray(SIZE * SIZE)
    alpha = bytearray(SIZE * SIZE)
    for i, (r, g, b, a) in enumerate(image.getdata()):
        gray[i] = (299 * r + 587 * g + 114 * b + 500) // 1000
        alpha[i] = a

    dst.write_bytes(bytes(gray) + bytes(alpha))
    expected = SIZE * SIZE * 2
    if dst.stat().st_size != expected:
        raise SystemExit(f"bad output size for {dst}: {dst.stat().st_size}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--src", type=pathlib.Path, required=True)
    parser.add_argument("--out", type=pathlib.Path, required=True)
    args = parser.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    for name in PIECES:
        src = args.src / f"{name}.svg"
        if not src.is_file():
            raise SystemExit(f"missing source artwork: {src}")
        rasterize(src, args.out / f"{name}.r8a8")

    print(f"rasterized {len(PIECES)} GNOME Chess pieces at {SIZE}x{SIZE}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
