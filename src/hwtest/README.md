# FlipOS Hardware Test

Native Miyoo Flip hardware validation application.

Target:
- Miyoo Flip
- RK3566
- Linux 5.10.160
- AArch64
- 640x480 display

Initial interfaces:
- DRM/KMS: /dev/dri/card0
- Digital input: /dev/input/event0
- Analogue UART: /dev/ttyS1
- Lid: hall sensor interface (integration pending)

Architecture:
- direct DRM/KMS
- dumb framebuffer
- no SDL
- no libdrm requirement
- direct Linux input_event handling
- built-in bitmap font

Known-good proof:
flip-kms-ui demonstrated direct KMS framebuffer setup and
/dev/input/event0 operation on physical Miyoo Flip hardware.
