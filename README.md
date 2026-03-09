# Arduino Nano Servo Metronome

This repository contains an Arduino sketch for a **servo-driven metronome stick** where tempo is controlled by **serial commands and/or two buttons** (no potentiometer required).

## File
- `servo_metronome_nano.ino`

## Wiring (Arduino Nano)
- Servo signal -> `D9`
- Servo power -> external 5V supply (recommended)
- Servo GND -> external GND
- Nano GND -> same external GND (common ground)

Optional button control:
- `D2` -> button -> `GND` (BPM up)
- `D3` -> button -> `GND` (BPM down)

Buttons use `INPUT_PULLUP`, so they are active when pressed to ground.

## Behavior
- BPM range is **40 to 1440**.
- Default BPM at startup is `120`.
- Servo swings around center angle (`90°`) by **medium travel** (`±30°`).
- Each full left-right cycle corresponds to one beat.

## Serial commands (115200 baud)
- `BPM 220` -> set BPM directly
- `220` -> set BPM directly
- `+` or `UP` -> BPM +1
- `-` or `DOWN` -> BPM -1
- `STATUS` -> print current BPM
- `HELP` -> print command help

## Tune for your mechanism
Adjust these constants in the sketch:
- `MIN_BPM`, `MAX_BPM`, `DEFAULT_BPM`
- `CENTER_ANGLE`, `SWING_ANGLE`
- `BUTTON_STEP_BPM`, `BUTTON_REPEAT_MS`

If your stick is heavy, reduce `SWING_ANGLE` and/or maximum BPM.
