# Kindle hardware lab

This directory contains bounded QA tooling for a real Kindle. It only operates on:

- `/mnt/us/extensions/kindlebrew-chess`
- `/mnt/us/kindlebrew-data/chess`
- `/mnt/us/documents/kindlebrew-qa/<run-id>`

The harness does not modify the root filesystem, framework internals, watchdogs, firmware, or the shared `gnomegames` extension. Process signals are sent only after checking the recorded PID, executable path, and process start time.

## Requirements

- Windows OpenSSH (`ssh.exe` and `scp.exe`) for all device traffic. WSL networking is intentionally not used.
- The Kindle `kindlehf` cross compiler.
- FBInk checked out at the same pinned revision used by the engine build.
- SSH key/agent authentication, or an interactively entered password. Passwords must never be stored in `config.local.psd1`, command arguments, or repository files.

Copy `config.example.psd1` to the ignored `config.local.psd1` only when the host, port, or user differ.

## Build helpers

From WSL, with the repository mounted under `/mnt/c`:

```sh
make -C tools/kindle-lab \
  KINDLE_PREFIX="$HOME/x-tools/arm-kindlehf-linux-gnueabihf/bin/arm-kindlehf-linux-gnueabihf-" \
  FBINK_DIR=/path/to/pinned/FBInk
```

The four small ARM helpers are written to the ignored `tools/kindle-lab/build/` directory.

## Typical run

Run these commands in Windows PowerShell. `Deploy` records its generated run id in the ignored build directory, so later commands can omit `-RunId`.

```powershell
$lab = '.\tools\kindle-lab\kindle-lab.ps1'
& $lab Connection
& $lab Deploy -Package .\build\artifacts\chess_2.0.0_qa.kpkg
& $lab Inventory
& $lab InstallHooks -Package .\build\artifacts\chess_2.0.0_qa.kpkg
& $lab InputStart
& $lab Run
& $lab Status
& $lab Tap -X 585 -Y 975 -DurationMs 30
& $lab Framebuffer
& $lab Stop
& $lab InputStop
& $lab Collect
```

`InstallHooks` deliberately names what it does: it validates the uploaded SHA-256 and invokes the extracted KPM install hook. It is not a claim that KPM itself installed the package. A true KPM clean-install/upgrade/uninstall test remains a separate release gate.

Framebuffer dumps are stable, packed Y8 snapshots. The helper takes two complete in-memory reads and retries a bounded number of times rather than writing rows while the display is changing. A correct logical dump does not prove that the physical e-ink panel is free of ghosting; record both observations.
