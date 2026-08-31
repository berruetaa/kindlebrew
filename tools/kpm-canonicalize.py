#!/usr/bin/env python3
"""Rewrite a KPM tar.gz with deterministic, extraction-safe metadata."""
from __future__ import annotations

import argparse
import gzip
import io
import os
import pathlib
import tarfile


def fail(message: str) -> "None":
    raise SystemExit(message)


def canonical_name(name: str) -> pathlib.PurePosixPath:
    if not name or "\\" in name or "\x00" in name:
        fail(f"invalid archive member name: {name!r}")
    path = pathlib.PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or str(path) != name.rstrip("/"):
        fail(f"unsafe or non-canonical archive member: {name!r}")
    return path


def is_text_member(name: str) -> bool:
    return pathlib.PurePosixPath(name).suffix.lower() in {
        ".html",
        ".json",
        ".md",
        ".sh",
        ".txt",
    }


def canonicalize(source: pathlib.Path, destination: pathlib.Path, epoch: int) -> None:
    with tarfile.open(source, "r:gz") as incoming:
        members = incoming.getmembers()
        names = [member.name.rstrip("/") for member in members]
        if not members or len(names) != len(set(names)):
            fail("empty package or duplicate archive member")

        destination.parent.mkdir(parents=True, exist_ok=True)
        with destination.open("wb") as raw:
            with gzip.GzipFile(
                filename="", mode="wb", compresslevel=5, fileobj=raw, mtime=0
            ) as compressed:
                with tarfile.open(
                    fileobj=compressed, mode="w", format=tarfile.USTAR_FORMAT
                ) as outgoing:
                    for member in sorted(members, key=lambda item: item.name.rstrip("/")):
                        name = str(canonical_name(member.name))
                        if not (member.isfile() or member.isdir()):
                            fail(f"unsupported special archive member: {member.name}")

                        normalized = tarfile.TarInfo(name=name)
                        normalized.type = (
                            tarfile.DIRTYPE if member.isdir() else tarfile.REGTYPE
                        )
                        normalized.mode = member.mode & 0o777
                        normalized.uid = 0
                        normalized.gid = 0
                        normalized.uname = ""
                        normalized.gname = ""
                        normalized.mtime = epoch
                        normalized.size = 0 if member.isdir() else member.size

                        if member.isdir():
                            outgoing.addfile(normalized)
                        else:
                            payload = incoming.extractfile(member)
                            if payload is None:
                                fail(f"cannot read archive member: {member.name}")
                            if is_text_member(name):
                                data = payload.read().replace(b"\r\n", b"\n").replace(b"\r", b"\n")
                                normalized.size = len(data)
                                outgoing.addfile(normalized, io.BytesIO(data))
                            else:
                                outgoing.addfile(normalized, payload)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()

    try:
        epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "0"), 10)
    except ValueError:
        fail("SOURCE_DATE_EPOCH must be an integer")
    if epoch < 0:
        fail("SOURCE_DATE_EPOCH must not be negative")

    canonicalize(args.source, args.destination, epoch)


if __name__ == "__main__":
    main()
