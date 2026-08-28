# Kindle input and lifecycle

## Never hard-code /dev/input/eventN

Event numbers are implementation details and have changed across Kindle generations and firmware.

KBGE uses FBInk input classification to discover touchscreens, scaled tablets, pagination buttons, Home/D-pad/Menu keys, new frame-tap devices and the power button.

FBInk already carries Kindle-specific heuristics for old kernels, zForce/Parade-style touch devices and unusual key codes.

## Exclusive touch

Amazon's UI remains alive while a game runs. To stop one physical tap from also activating the UI underneath the game, KBGE optionally applies EVIOCGRAB to touch devices.

The power button is never grabbed.

## Coordinates

Raw ABS ranges are queried with EVIOCGABS and normalized before mapping to the game canvas. KBGE then applies FBInk's touch-swap and mirror quirks.

Framebuffer rotation is deliberately not guessed from Kobo-style conversion helpers: FBInk explicitly documents those helpers as Kobo-only. Kindle gyro/orientation support should be based on observed Kindle rotation events and tested mappings.

## Multi-touch

Type-B multitouch is parsed through ABS_MT_SLOT, ABS_MT_TRACKING_ID, ABS_MT_POSITION_X/Y and SYN_REPORT. The public event includes a touch ID. Single-touch panels use slot zero.

## Gestures

The backend synthesizes tap, double-tap, hold and swipe on top of raw down/move/up events. Thresholds are configurable and the defaults derive from screen DPI.

## Power and screensaver

A normal game requests:

~~~sh
lipc-set-prop com.lab126.powerd preventScreenSaver 1
~~~

and restores the property to zero on exit.

This is intentionally narrower than stopping powerd, changing global suspend policy or continuously faking user activity. The physical power button remains owned by Kindle services.

## Framework coexistence

Default policy: do not stop Amazon framework services.

This preserves power/frontlight/suspend behavior, reduces firmware assumptions and makes failure recovery much safer. The game draws over the framebuffer and temporarily owns the touchscreen. On shutdown:

~~~sh
/usr/bin/xrefresh -d :0.0
~~~

asks the native UI to repaint.

An explicit exclusive-framework mode may be added for a proven need, but service stop/start is not a sane default for games.

## Signals

SIGINT, SIGTERM and SIGHUP only set a quit flag. The event loop converts that flag into KB_EVENT_QUIT, so cleanup happens from normal process context rather than doing unsafe system calls inside a signal handler.
