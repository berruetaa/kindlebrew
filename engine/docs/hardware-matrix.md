# KBGE hardware validation matrix

Compilation is not hardware validation. This matrix separates what CI proves from what has physically been observed.

| Area | CI / static status | Physical-device status |
|---|---|---|
| Gray8 canvas / damage | unit tested | pending broader matrix |
| A2/DU/GL16/GC16 scheduling | policy unit tested + FBInk API build | requires InkLab visual pass per device family |
| kindlehf ABI | verified ELF32 ARM EABI5 | compatible target class; device smoke test still required |
| Touch discovery | FBInk API compiled | InkLab required |
| MTK fast mode | API compiled, restored on shutdown | InkLab latency/ghosting pass required |
| Suspend/resume | LIPC path compiled | power-button + cover pass required |
| Rotation | conservative raw gyro handling compiled | Oasis/Scribe matrix required |
| Direct Y8 mmap path | capability guarded | compare against FBInk fallback on hardware |
| Ink 2048 | logic tests + ARM build + package validation | playthrough smoke test required |

## InkLab acceptance pass

For each supported device/firmware family:

1. launch InkLab from KPM;
2. verify all touch corners and the center crosshair;
3. drag continuously and confirm low-latency updates do not leave pathological artifacts;
4. stop dragging and inspect the grayscale cleanup;
5. double-tap/swipe to force clean transitions;
6. suspend and resume with the power button;
7. where present, test cover/Hall suspend;
8. where present, rotate through supported orientations;
9. exit and verify the Amazon UI receives touch normally;
10. attach Kindlebrew-InkLab-diagnostics.txt to any compatibility report.

## Promotion rule

A backend capability can be considered broadly supported only after both CI and at least one physical InkLab pass on the relevant hardware/driver family.
