# Research map and pinned dependencies

This file records the concrete ecosystem assumptions behind KBGE so they are auditable instead of becoming folklore.

## Jailbreak / KMC

- Repository: KindleModding/jb.sh
- KMC installs FBInk binaries and libraries under /var/local/kmc.
- Stable symlinks are exposed through /var/local/kmc/bin and /var/local/kmc/lib.
- Current jailbreak build scripts install fbink, input_scan, fbdepth, libfbink.so and libfbink_input.so.

KBGE does not rely on those shared-library versions at runtime; its native test binary statically links a pinned FBInk build. The installed tools remain useful for diagnosis.

## FBInk

- Repository: KindleModding/FBInk
- Engine pin: 76557104a55ea163769464e59668e26f6af299ef
- KMC/KPM ecosystem identifies this line as FBInk 1.25.x.

APIs used directly:
- fbink_open / fbink_close
- fbink_init / fbink_get_state
- fbink_get_fb_pointer
- fbink_print_raw_data
- fbink_refresh_rect
- fbink_wait_for_complete
- fbink_input_scan

State fields consumed:
- screen/view dimensions and scanline stride
- bpp and pixel_format
- inverted_grayscale
- device name/codename/platform/DPI
- MediaTek capability
- wait reliability
- touch swap/mirror quirks
- viewport origins

## koxtoolchain / Kindle SDK

- Toolchain target: kindlehf
- Compiler triplet: arm-kindlehf-linux-gnueabihf
- KindleModding koxtoolchain release pinned by CI: 2026.04
- kindlehf archive SHA-256: d08d3a5cbbaa184cc0cc2279df1d64a71656ab2c1e0223cae324b8d30c8d73e0

The produced InkLab ELF is verified as ARM EABI5 and uses /lib/ld-linux-armhf.so.3. FBInk is not a dynamic dependency; only libc and libm remain dynamic in the current build.

## sh_integration

- Repository: KindleModding/sh_integration
- Version inspected: 4.1.0
- Commit: adb7ef5c570a25c76fd64e94837ae0f590bd3b9c

The extractor reads only the first six script lines for metadata. Supported directives used by Kindlebrew are Name, Author, Icon and DontUseFBInk.

Icons may be embedded as data:image/...;base64, which sh_integration extracts into the Scriptlet SDR directory before registering the content item.

## KPM

- KPM platform: kindlehf
- Games use manifest v2.
- install/launch/uninstall hooks operate from user-writable package extraction paths.
- Native game packages must not mutate rootfs.

## KOReader cross-checks

KOReader's Kindle backend independently confirms the same architectural choices:
- input devices are discovered through fbink_input_scan instead of fixed event numbers;
- Kindle-specific power and input behavior varies materially by generation;
- rotation devices are treated separately because generic ABS events are ambiguous.

KBGE intentionally follows the ecosystem's proven device-discovery approach rather than inventing a second Kindle model table.

## Runtime commands used

Keep screen awake while a game owns the display:

~~~sh
lipc-set-prop com.lab126.powerd preventScreenSaver 1
~~~

Restore normal behavior:

~~~sh
lipc-set-prop com.lab126.powerd preventScreenSaver 0
~~~

Ask X/native UI to repaint after direct-framebuffer use:

~~~sh
/usr/bin/xrefresh -d :0.0
~~~

These are intentionally narrow operations. KBGE does not stop powerd, remount rootfs, or replace Amazon libraries.
