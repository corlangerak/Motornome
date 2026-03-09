# Arduino Servo Metronome (Nano + ESP32)

This repository contains an Arduino sketch for a **servo-driven metronome stick** where tempo is controlled by **serial commands and/or two buttons** (no potentiometer required).

## File
- `servo_metronome_nano.ino`

## Wiring
- Servo signal -> `D9` on Nano (or change `SERVO_PIN` for ESP32, e.g. `18`)
- Servo power -> external 5V supply (recommended)
- Servo GND -> external GND
- Nano GND -> same external GND (common ground)

Optional button control:
- `D2` -> button -> `GND` (BPM up)
- `D3` -> button -> `GND` (BPM down)

Buttons use `INPUT_PULLUP`, so they are active when pressed to ground.


## Board compatibility
- **Arduino Nano / AVR**: uses `Servo.h` (built in with many AVR setups).
- **ESP32**: uses `ESP32Servo.h` automatically via `#if defined(ARDUINO_ARCH_ESP32)`.

If you compile for ESP32, install the **ESP32Servo** library from Library Manager.

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
