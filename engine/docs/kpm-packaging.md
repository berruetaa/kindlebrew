# KPM packaging contract

Kindlebrew treats `.kpkg` compatibility as a release invariant, not as an implementation detail.

## Error 8

KPM's result enum defines value 8 as `KPM_LIBARCHIVE_ERROR`. It is returned when libarchive cannot open, parse or extract a package archive.

KPM 0.2.x uses libarchive to read all formats/filters, but Kindlebrew v2 packages follow the format emitted by the official KPM helper: a gzip-compressed tar archive with `manifest.json` and package lifecycle scripts at the archive root.

Do not build Kindlebrew packages with ad-hoc GNU `tar`/`gzip` commands.

## Single packer

All Kindlebrew package workflows must call:

~~~sh
bash tools/kpm-pack.sh <staging-directory> <output.kpkg>
~~~

The wrapper pins the `kpm-helper.py` from KPM 0.2.2 (`799adf...`), the revision currently shipped by Vera/jb.sh, asks it to perform `package pack`, then validates the result independently.

Current validation gates:

- gzip magic for manifest-v2 artifacts;
- root `manifest.json`;
- no duplicate archive members;
- no absolute paths or `..` traversal;
- no escaping symlink/hardlink targets;
- no KPM-reserved `rootfs` or `startup.sh` root entries;
- manifest version/id/version shape checks;
- independent libarchive listing with `bsdtar`;
- full libarchive extraction to a temporary directory;
- root manifest exists after extraction;
- lifecycle shell scripts contain no CR/CRLF;
- SHA-256 printed for the produced artifact.

## Why validation is stricter than KPM

At the pinned KPM revision, the package installer invokes `Internal_ExtractArchive` and does not check that function's return value at that call site before continuing to inspect/run package files. That makes producer-side validation especially valuable: a package should never reach the device if libarchive cannot extract it cleanly.

## Staging layout

A normal v2 package looks like:

~~~text
manifest.json
install.sh
launch.sh
uninstall.sh
LICENSE
SOURCE.txt
payload/
~~~

Additional ordinary assets such as `cover.png` or generated library integration scripts are allowed. Keep the package root intentionally small and deterministic.

## Workflow rule

Do not reimplement package creation inside an individual GitHub Actions workflow. If packaging behavior needs to change, change `tools/kpm-pack.sh` and let every package workflow consume the same implementation.
