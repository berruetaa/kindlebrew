# Platform, ABI and packaging

## kindlehf

Modern hard-float Kindle packages target KPM platform kindlehf and toolchain arm-kindlehf-linux-gnueabihf. The current Kindle SDK documentation maps kindlehf to firmware 5.16.3 and newer.

Do not silently substitute Ubuntu's generic arm-linux-gnueabihf compiler/sysroot. That can introduce libc or loader requirements newer than the target Kindle firmware.

CI pins a KindleModding koxtoolchain archive and verifies its SHA-256 before use.

## FBInk

Vera/KMC already ships FBInk binaries and libraries. KBGE nevertheless builds against a pinned FBInk source revision and statically links the engine smoke-test binary.

This avoids accidental runtime libfbink ABI drift and ensures the compiled feature set is exactly the one the engine expects.

The engine core needs the draw and input feature sets. Runtime PNG decoding/OpenType are deliberately outside the hot rendering path; games should preprocess assets to Gray8 where practical.

## Why Gray8 assets

Gray8 maps directly to luminance, makes B/W-vs-grayscale damage classification cheap, avoids RGB-to-gray conversion during gameplay, and can be copied directly on an 8bpp framebuffer.

Scaling, alpha compositing and color conversion are better build-time work than e-reader runtime work.

## KPM package rules

A native game package should:
- declare supported_platforms as kindlehf;
- install only into user-writable locations;
- never remount or modify rootfs;
- provide install.sh, launch.sh and uninstall.sh;
- install a Scriptlet in /mnt/us/documents when a library entry is desired.

## Scriptlet metadata

sh_integration reads metadata from the first few header lines:

~~~sh
#!/bin/sh
# Name: Game Name
# Author: Author / Kindlebrew
# Icon: data:image/png;base64,...
# DontUseFBInk
~~~

Embedding a small cover avoids path races during indexing. The Scriptlet should usually delegate to KPM:

~~~sh
/var/local/kmc/bin/kpm launch package-id
~~~

## No rootfs writes

Normal game startup must not remount /, replace Amazon libraries, install into /usr or patch services.

If a compatibility shim is required, ship it next to the game and select it with a private LD_LIBRARY_PATH, as Kindlebrew does for GNOME Chess.
