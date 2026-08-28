# Library covers and Scriptlets

Kindlebrew games appear in the stock Kindle library through sh_integration Scriptlets. The library cover belongs to that integration layer, not to the KBGE runtime and not to the KPM repository manifest.

## Per-game cover convention

A native game declares its stock-library metadata in `library.json` and may define one optional cover asset beside it:

~~~text
package-src/my-game/
├── manifest.json
├── install.sh
├── launch.sh
├── uninstall.sh
└── cover.png       # optional
~~~

Supported Kindlebrew cover names are:

- `cover.png`
- `cover.png`
- `cover.jpg`
- `cover.jpeg`

Use at most one.

During package assembly the cover is copied into the `.kpkg`. During installation it is copied into the game's persistent extension directory, for example:

~~~text
/mnt/us/extensions/kindlebrew-my-game/cover.png
~~~

The library Scriptlet then references that absolute path:

~~~sh
#!/bin/sh
# Name: My Game
# Author: Me
# Icon: /mnt/us/extensions/kindlebrew-my-game/cover.png
# DontUseFBInk
exec /var/local/kmc/bin/kpm launch my-game "$@"
~~~

If a game ships no cover, the `# Icon:` line is simply omitted and Kindle/sh_integration uses its normal fallback presentation.

## Why the Scriptlet is generated

`sh_integration` v4.1.0 only scans the first six physical lines when reading Scriptlet metadata. Hand-written launchers are therefore surprisingly fragile: adding comments above `# Icon:` can make a cover silently disappear.

`tools/render-game-scriptlet.py` generates the header and refuses malformed metadata. With an icon, all metadata plus the launch command still fit in those six lines.

Example:

~~~sh
python tools/render-game-scriptlet.py \
  --name 'My Game' \
  --author 'Me' \
  --package-id my-game \
  --install-dir /mnt/us/extensions/kindlebrew-my-game \
  --cover-filename cover.png \
  --output build/my-game/scriptlet.sh
~~~

## Why an absolute image path is preferred

`sh_integration` accepts either a `data:image/...;base64,...` value or a readable/writable filesystem path. An absolute installed image path keeps the Scriptlet tiny, avoids huge base64 header lines, and lets a package replace its cover during an upgrade.

Base64 remains valid for tiny self-contained Scriptlets; InkLab intentionally keeps exercising that route.

## Updating a cover

An upgrade should copy the new cover first and rewrite/copy the Scriptlet second. The Scriptlet modification causes the Kindle scanner to process an update; sh_integration removes the old catalog entry/SDR metadata and indexes the current `# Icon:` value again.

Do not write directly to `cc.db`. That duplicates sh_integration behavior and couples the package to Amazon's internal database schema.

## Cover design recommendations

Treat the asset as an e-ink thumbnail first: strong silhouette, high local contrast, minimal fine texture, and a design that survives grayscale. Use PNG or JPEG for the packaged asset. SVG may be useful as a design-time source, but Kindlebrew does not claim stock-library SVG thumbnail compatibility without device evidence.

## Scriptlet filename stability

`document_name` is part of the installed-library identity and should normally remain stable across releases. If a release intentionally renames it, declare every older managed name in `legacy_document_names`:

~~~json
{
  "schema_version": 1,
  "name": "My Game",
  "author": "Me",
  "document_name": "My-Game.sh",
  "legacy_document_names": ["Old-My-Game.sh"],
  "cover": "cover.png"
}
~~~

The generated library helper removes those old Scriptlets and their `.sdr` metadata during both upgrade and uninstall. Names are validated as simple `.sh` filenames; paths and duplicates are rejected.
