#!/usr/bin/env python3
"""Generate sh_integration launchers and stage Kindlebrew library metadata."""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import shutil

SAFE_ID = re.compile(r"^[a-z0-9_-]+$")
SAFE_COVER = re.compile(r"^cover\.(?:png|jpe?g)$", re.IGNORECASE)
SAFE_DOCUMENT = re.compile(r"^[A-Za-z0-9._ -]+\.sh$")


def clean_metadata(value: str, field: str) -> str:
    value = value.strip()
    if not value or "\n" in value or "\r" in value:
        raise SystemExit(f"invalid {field}")
    return value


def validate_package_id(package_id: str) -> str:
    if not SAFE_ID.fullmatch(package_id):
        raise SystemExit(
            "package-id must contain only lowercase ASCII letters, digits, _ or -"
        )
    return package_id


def validate_cover_filename(value: str | None) -> str | None:
    if value is None:
        return None
    cover = pathlib.PurePosixPath(value)
    if cover.name != value or not SAFE_COVER.fullmatch(cover.name):
        raise SystemExit(
            "cover filename must be cover.png, cover.jpg or cover.jpeg"
        )
    return cover.name


def render_scriptlet(
    *,
    name: str,
    author: str,
    package_id: str,
    install_dir: str,
    cover_filename: str | None,
) -> str:
    name = clean_metadata(name, "name")
    author = clean_metadata(author, "author")
    package_id = validate_package_id(package_id)
    cover_filename = validate_cover_filename(cover_filename)

    install_dir = install_dir.rstrip("/")
    if (
        not install_dir.startswith("/mnt/us/")
        or "\n" in install_dir
        or "\r" in install_dir
    ):
        raise SystemExit("install-dir must be an absolute /mnt/us path")

    lines = [
        "#!/bin/sh",
        f"# Name: {name}",
        f"# Author: {author}",
    ]

    if cover_filename:
        lines.append(f"# Icon: {install_dir}/{cover_filename}")

    lines.extend(
        [
            "# DontUseFBInk",
            f'exec /var/local/kmc/bin/kpm launch {package_id} "$@"',
        ]
    )

    # sh_integration v4.1.0 only scans the first six physical lines.
    metadata = lines[:6]
    required = ["# Name: ", "# Author: ", "# DontUseFBInk"]
    for prefix in required:
        if not any(line.startswith(prefix) for line in metadata):
            raise SystemExit(
                f"{prefix.strip()} fell outside sh_integration's 6-line header"
            )
    if cover_filename and not any(
        line.startswith("# Icon: ") for line in metadata
    ):
        raise SystemExit("# Icon fell outside sh_integration's 6-line header")

    if len(lines) > 6:
        raise SystemExit("generated Scriptlet exceeded sh_integration's 6-line contract")

    return "\n".join(lines) + "\n"


def write_scriptlet(path: pathlib.Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")
    print(f"generated {path}")


def render_library_installer(
    *,
    package_id: str,
    document_name: str,
    legacy_document_names: list[str],
    cover_filename: str | None,
) -> str:
    install_dir = f"/mnt/us/extensions/kindlebrew-{package_id}"
    document_path = f"/mnt/us/documents/{document_name}"
    cover_value = cover_filename or ""
    managed_exec = f'exec /var/local/kmc/bin/kpm launch {package_id} "$@"'
    legacy_cleanup = []
    for name in legacy_document_names:
        path = f"/mnt/us/documents/{name}"
        legacy_cleanup.append(f'    remove_managed_document "{path}"')
    legacy_cleanup_text = "\n".join(legacy_cleanup) if legacy_cleanup else "    :"

    # Every interpolated value has already passed strict filename/id validation.
    return f"""#!/bin/sh
set -eu

TARGET='{install_dir}'
DOC='{document_path}'
COVER='{cover_value}'
EXPECTED_EXEC='{managed_exec}'

is_managed_document() {{
    [ -f "$1" ] &&
        grep -Fqx '# DontUseFBInk' "$1" &&
        grep -Fqx "$EXPECTED_EXEC" "$1"
}}

remove_managed_document() {{
    path="$1"
    [ ! -e "$path" ] && return 0
    if is_managed_document "$path"; then
        rm -f "$path"
        rm -rf "$path.sdr"
    else
        echo "Preserving unmanaged Kindle document: $path" >&2
    fi
}}

cleanup_legacy_docs() {{
{legacy_cleanup_text}
}}

cleanup_staging() {{
    rm -f "$DOC.kindlebrew-new.$$" 2>/dev/null || true
    if [ -n "$COVER" ]; then
        rm -f "$TARGET/$COVER.kindlebrew-new.$$" 2>/dev/null || true
    fi
}}

install_library() {{
    if [ ! -f scriptlet.sh ]; then
        echo 'Kindlebrew library Scriptlet is missing.' >&2
        exit 1
    fi
    if [ -n "$COVER" ] && [ ! -f "$COVER" ]; then
        echo "Declared Kindlebrew cover is missing: $COVER" >&2
        exit 1
    fi
    if [ -e "$DOC" ] && ! is_managed_document "$DOC"; then
        echo "Existing Kindle document is unmanaged; refusing to overwrite: $DOC" >&2
        exit 1
    fi

    mkdir -p "$TARGET" /mnt/us/documents
    cleanup_staging

    doc_tmp="$DOC.kindlebrew-new.$$"
    if ! cp scriptlet.sh "$doc_tmp"; then
        cleanup_staging
        exit 1
    fi
    chmod 755 "$doc_tmp" 2>/dev/null || true

    # If a cover is added/replaced, make the final image path valid before the
    # new Scriptlet can reference it. Old cover formats remain in place until
    # after the document swap, so the previous Scriptlet remains valid too.
    if [ -n "$COVER" ]; then
        cover_tmp="$TARGET/$COVER.kindlebrew-new.$$"
        if ! cp "$COVER" "$cover_tmp"; then
            cleanup_staging
            exit 1
        fi
        # sh_integration validates path icons with access(R_OK|W_OK).
        chmod 666 "$cover_tmp" 2>/dev/null || true
        if ! mv -f "$cover_tmp" "$TARGET/$COVER"; then
            cleanup_staging
            exit 1
        fi
    fi

    # Same-filesystem rename is the commit point for library metadata.
    if ! mv -f "$doc_tmp" "$DOC"; then
        cleanup_staging
        exit 1
    fi

    # Everything below is post-commit hygiene and must not turn a successful
    # visible update into a KPM install failure.
    cleanup_legacy_docs
    for stale in cover.png cover.jpg cover.jpeg; do
        if [ "$stale" != "$COVER" ]; then
            rm -f "$TARGET/$stale" 2>/dev/null || true
        fi
    done
}}

uninstall_library() {{
    cleanup_staging
    cleanup_legacy_docs
    remove_managed_document "$DOC"
}}

case "${{1:-install}}" in
    install) install_library ;;
    uninstall) uninstall_library ;;
    *) echo 'usage: library-install.sh [install|uninstall]' >&2; exit 2 ;;
esac
"""


def stage_package_library(package_source: pathlib.Path, stage_dir: pathlib.Path) -> None:
    manifest_path = package_source / "manifest.json"
    library_path = package_source / "library.json"
    if not manifest_path.is_file():
        raise SystemExit(f"missing package manifest: {manifest_path}")
    if not library_path.is_file():
        raise SystemExit(f"missing Kindlebrew library metadata: {library_path}")

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    library = json.loads(library_path.read_text(encoding="utf-8"))

    if manifest.get("manifest_version") != 2:
        raise SystemExit("native-game library integration requires KPM manifest v2")

    package_id = validate_package_id(str(manifest.get("id", "")))
    if library.get("schema_version") != 1:
        raise SystemExit("unsupported library.json schema_version")

    name = clean_metadata(str(library.get("name", "")), "name")
    author = clean_metadata(str(library.get("author", "")), "author")

    document_name = str(library.get("document_name", ""))
    if (
        not SAFE_DOCUMENT.fullmatch(document_name)
        or pathlib.PurePosixPath(document_name).name != document_name
    ):
        raise SystemExit("document_name must be a simple .sh filename")

    raw_legacy = library.get("legacy_document_names", [])
    if not isinstance(raw_legacy, list) or len(raw_legacy) > 16:
        raise SystemExit("legacy_document_names must be an array of at most 16 names")
    legacy_document_names: list[str] = []
    for value in raw_legacy:
        if not isinstance(value, str):
            raise SystemExit("legacy_document_names entries must be strings")
        if (
            not SAFE_DOCUMENT.fullmatch(value)
            or pathlib.PurePosixPath(value).name != value
        ):
            raise SystemExit("legacy_document_names entries must be simple .sh filenames")
        if value == document_name:
            raise SystemExit("document_name must not appear in legacy_document_names")
        if value in legacy_document_names:
            raise SystemExit("legacy_document_names must not contain duplicates")
        legacy_document_names.append(value)

    cover_filename = validate_cover_filename(library.get("cover"))
    discovered = sorted(
        p.name for p in package_source.iterdir() if SAFE_COVER.fullmatch(p.name)
    )
    if cover_filename is None and discovered:
        raise SystemExit(
            "cover asset exists but library.json does not explicitly declare it"
        )
    if cover_filename is not None:
        if discovered != [cover_filename]:
            raise SystemExit(
                "library.json cover must name the one and only cover.* asset"
            )
        if not (package_source / cover_filename).is_file():
            raise SystemExit(f"declared cover is missing: {cover_filename}")

    install_dir = f"/mnt/us/extensions/kindlebrew-{package_id}"
    scriptlet = render_scriptlet(
        name=name,
        author=author,
        package_id=package_id,
        install_dir=install_dir,
        cover_filename=cover_filename,
    )

    stage_dir.mkdir(parents=True, exist_ok=True)
    write_scriptlet(stage_dir / "scriptlet.sh", scriptlet)
    write_scriptlet(
        stage_dir / "library-install.sh",
        render_library_installer(
            package_id=package_id,
            document_name=document_name,
            legacy_document_names=legacy_document_names,
            cover_filename=cover_filename,
        ),
    )

    # Keep the declaration in the package for diagnostics/future tooling.
    shutil.copy2(library_path, stage_dir / "library.json")
    if cover_filename:
        shutil.copy2(package_source / cover_filename, stage_dir / cover_filename)

    print(
        json.dumps(
            {
                "package_id": package_id,
                "document_name": document_name,
                "legacy_document_names": legacy_document_names,
                "install_dir": install_dir,
                "cover": cover_filename,
            },
            sort_keys=True,
        )
    )


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--package-source", type=pathlib.Path)
    p.add_argument("--stage-dir", type=pathlib.Path)

    p.add_argument("--name")
    p.add_argument("--author")
    p.add_argument("--package-id")
    p.add_argument("--install-dir")
    p.add_argument("--cover-filename")
    p.add_argument("--output", type=pathlib.Path)
    args = p.parse_args()

    if args.package_source is not None or args.stage_dir is not None:
        if args.package_source is None or args.stage_dir is None:
            raise SystemExit("--package-source and --stage-dir must be used together")
        legacy = [
            args.name,
            args.author,
            args.package_id,
            args.install_dir,
            args.cover_filename,
            args.output,
        ]
        if any(value is not None for value in legacy):
            raise SystemExit("metadata staging mode cannot be mixed with direct arguments")
        stage_package_library(args.package_source, args.stage_dir)
        return

    required = {
        "--name": args.name,
        "--author": args.author,
        "--package-id": args.package_id,
        "--install-dir": args.install_dir,
        "--output": args.output,
    }
    missing = [flag for flag, value in required.items() if value is None]
    if missing:
        raise SystemExit("missing required arguments: " + ", ".join(missing))

    text = render_scriptlet(
        name=args.name,
        author=args.author,
        package_id=args.package_id,
        install_dir=args.install_dir,
        cover_filename=args.cover_filename,
    )
    write_scriptlet(args.output, text)


if __name__ == "__main__":
    main()
