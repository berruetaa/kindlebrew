#!/usr/bin/env python3
"""Generate a sh_integration launcher for a Kindlebrew game."""
from __future__ import annotations

import argparse
import pathlib
import re

SAFE_ID = re.compile(r"^[a-z0-9_-]+$")
SAFE_COVER = re.compile(r"^cover\.(?:svg|png|jpe?g)$", re.IGNORECASE)


def clean_metadata(value: str, field: str) -> str:
    value = value.strip()
    if not value or "\n" in value or "\r" in value:
        raise SystemExit(f"invalid {field}")
    return value


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--name", required=True)
    p.add_argument("--author", required=True)
    p.add_argument("--package-id", required=True)
    p.add_argument("--install-dir", required=True)
    p.add_argument("--cover-filename")
    p.add_argument("--output", required=True, type=pathlib.Path)
    args = p.parse_args()

    name = clean_metadata(args.name, "name")
    author = clean_metadata(args.author, "author")
    if not SAFE_ID.fullmatch(args.package_id):
        raise SystemExit("package-id must contain only lowercase ASCII letters, digits, _ or -")

    install_dir = args.install_dir.rstrip("/")
    if not install_dir.startswith("/mnt/us/") or "\n" in install_dir or "\r" in install_dir:
        raise SystemExit("install-dir must be an absolute /mnt/us path")

    lines = [
        "#!/bin/sh",
        f"# Name: {name}",
        f"# Author: {author}",
    ]

    if args.cover_filename:
        cover = pathlib.PurePosixPath(args.cover_filename)
        if cover.name != args.cover_filename or not SAFE_COVER.fullmatch(cover.name):
            raise SystemExit("cover filename must be cover.svg, cover.png, cover.jpg or cover.jpeg")
        lines.append(f"# Icon: {install_dir}/{cover.name}")

    lines.extend([
        "# DontUseFBInk",
        f'exec /var/local/kmc/bin/kpm launch {args.package_id} "$@"',
    ])

    # sh_integration v4.1.0 only scans the first six physical lines.
    metadata = lines[:6]
    required = ["# Name: ", "# Author: ", "# DontUseFBInk"]
    for prefix in required:
        if not any(line.startswith(prefix) for line in metadata):
            raise SystemExit(f"{prefix.strip()} fell outside sh_integration's 6-line header")
    if args.cover_filename and not any(line.startswith("# Icon: ") for line in metadata):
        raise SystemExit("# Icon fell outside sh_integration's 6-line header")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print(f"generated {args.output}")


if __name__ == "__main__":
    main()
