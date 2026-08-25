# Miyoo Flip Power / Audio Findings

## Confirmed hardware

Device:
- Miyoo Flip
- RK3566
- Linux 5.10.160

## Lid / Hall sensor

Persistent lid state:

    /sys/devices/platform/hall-mh248/hallvalue

Values:

    1 = lid open
    0 = lid closed

Hall interrupt:

    IRQ 72
    hall_mh248

Observed close/open transitions increment IRQ 72.

## Suspend

Available:

    /sys/power/state
    freeze mem

Suspend mode:

    /sys/power/mem_sleep
    s2idle [deep]

Confirmed working sequence:

    echo deep > /sys/power/mem_sleep
    echo mem > /sys/power/state

Opening the lid wakes RK3566 from deep suspend.

A 20+ minute deep-suspend test completed successfully without
spontaneous wake.

## Backlight

Device:

    /sys/class/backlight/backlight

Confirmed:

    brightness=0 turns backlight off
    restoring previous value restores brightness

## Speaker / RK817

ALSA card:

    card 0: rockchip-rk817

PCM:

    hw:0,0
    S16_LE
    stereo
    32768 Hz

External speaker control:

    Device-tree property:
        spk-ctl-gpios

    GPIO:
        gpio4-18
        global GPIO 146
        label spk-ctl

During active playback:

    gpio146 = HIGH
    fe410000.i2s = active
    PCM = RUNNING

## RK817 deep-resume audio failure

Confirmed failure after RK3566 deep suspend:

- PCM opens successfully
- PCM is RUNNING
- I2S is active
- speaker GPIO146 is HIGH
- ALSA mixer reports Speaker=on
- Playback Path=SPK
- physical speaker remains silent

Known-good RK817 codec registers during audible playback:

    0x17 = 0x40
    0x38 = 0x10
    0x40 = 0xA5

Confirmed broken post-deep-resume state:

    0x17 = 0x06
    0x38 = 0x00
    0x40 = 0x69

Restoring the three known-good values while PCM playback was
already running immediately restored audible speaker output.

FlipOS currently detects the exact broken signature and applies
the three-register recovery from the global flip_audio layer.

## Current power/session behavior

Game lid close:

    save state
    -> emulator exits with suspend result
    -> session marked suspended
    -> FlipOS enters RK3566 deep suspend

Lid open:

    hall wake
    -> FlipOS resumes
    -> RK817 audio recovery if required
    -> emulator relaunch
    -> state restore
    -> gameplay and sound resume

Frontend HOME/GAMES/RECENT also enter global deep suspend on lid
close.

## Known UX issue

Volume currently resets across emulator process relaunch because
flip_audio.c initializes:

    speaker_volume = 10

Volume persistence is intentionally deferred for later cleanup.
