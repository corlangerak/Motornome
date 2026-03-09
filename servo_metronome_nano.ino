#include <Servo.h>

// ----------------------
// Hardware configuration
// ----------------------
constexpr uint8_t SERVO_PIN = 9;          // Nano pin for servo signal
constexpr uint8_t BTN_UP_PIN = 2;         // Button: increase BPM (to GND)
constexpr uint8_t BTN_DOWN_PIN = 3;       // Button: decrease BPM (to GND)

// ----------------------
// Motion configuration
// ----------------------
constexpr int CENTER_ANGLE = 90;          // Neutral point
constexpr int SWING_ANGLE = 30;           // Medium swing (+/- from center)
constexpr int LEFT_ANGLE = CENTER_ANGLE - SWING_ANGLE;
constexpr int RIGHT_ANGLE = CENTER_ANGLE + SWING_ANGLE;

// ----------------------
// Tempo configuration
// ----------------------
constexpr int MIN_BPM = 40;
constexpr int MAX_BPM = 1440;
constexpr int DEFAULT_BPM = 120;
constexpr int BUTTON_STEP_BPM = 5;

constexpr unsigned long BUTTON_DEBOUNCE_MS = 25;
constexpr unsigned long BUTTON_REPEAT_MS = 120; // hold-to-repeat interval

Servo metronomeServo;

unsigned long lastStepMs = 0;
unsigned long halfBeatMs = 250; // updated from currentBpm
bool goRight = true;
int currentBpm = DEFAULT_BPM;

bool lastUpStablePressed = false;
bool lastDownStablePressed = false;
unsigned long upLastChangeMs = 0;
unsigned long downLastChangeMs = 0;
unsigned long upLastRepeatMs = 0;
unsigned long downLastRepeatMs = 0;

void setBpm(int bpm) {
  currentBpm = constrain(bpm, MIN_BPM, MAX_BPM);
  halfBeatMs = 60000UL / (unsigned long)currentBpm / 2UL;

  Serial.print(F("BPM set to "));
  Serial.println(currentBpm);
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  BPM <value>  -> set BPM (40..1440), e.g. BPM 220"));
  Serial.println(F("  +            -> increase BPM by 1"));
  Serial.println(F("  -            -> decrease BPM by 1"));
  Serial.println(F("  STATUS       -> print current BPM"));
  Serial.println(F("  HELP         -> show this help"));
}

void handleSerial() {
  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (line.equalsIgnoreCase("+") || line.equalsIgnoreCase("UP")) {
    setBpm(currentBpm + 1);
    return;
  }

  if (line.equalsIgnoreCase("-") || line.equalsIgnoreCase("DOWN")) {
    setBpm(currentBpm - 1);
    return;
  }

  if (line.equalsIgnoreCase("STATUS")) {
    Serial.print(F("Current BPM: "));
    Serial.println(currentBpm);
    return;
  }

  if (line.equalsIgnoreCase("HELP")) {
    printHelp();
    return;
  }

  // Accept "BPM 220" or plain numeric value "220"
  if (line.startsWith("BPM") || line.startsWith("bpm")) {
    line.remove(0, 3);
    line.trim();
  }

  if (line.length() > 0) {
    int value = line.toInt();
    if (value >= MIN_BPM && value <= MAX_BPM) {
      setBpm(value);
      return;
    }
  }

  Serial.println(F("Invalid command. Type HELP."));
}

bool readButtonPressed(uint8_t pin) {
  return digitalRead(pin) == LOW; // INPUT_PULLUP, pressed when LOW
}

void handleButtons() {
  unsigned long now = millis();

  bool upPressed = readButtonPressed(BTN_UP_PIN);
  bool downPressed = readButtonPressed(BTN_DOWN_PIN);

  if (upPressed != lastUpStablePressed && (now - upLastChangeMs) > BUTTON_DEBOUNCE_MS) {
    lastUpStablePressed = upPressed;
    upLastChangeMs = now;
    if (upPressed) {
      setBpm(currentBpm + BUTTON_STEP_BPM);
      upLastRepeatMs = now;
    }
  }

  if (downPressed != lastDownStablePressed && (now - downLastChangeMs) > BUTTON_DEBOUNCE_MS) {
    lastDownStablePressed = downPressed;
    downLastChangeMs = now;
    if (downPressed) {
      setBpm(currentBpm - BUTTON_STEP_BPM);
      downLastRepeatMs = now;
    }
  }

  if (lastUpStablePressed && (now - upLastRepeatMs) > BUTTON_REPEAT_MS) {
    setBpm(currentBpm + BUTTON_STEP_BPM);
    upLastRepeatMs = now;
  }

  if (lastDownStablePressed && (now - downLastRepeatMs) > BUTTON_REPEAT_MS) {
    setBpm(currentBpm - BUTTON_STEP_BPM);
    downLastRepeatMs = now;
  }
}

void updateSwing() {
  unsigned long now = millis();
  if (now - lastStepMs < halfBeatMs) return;

  lastStepMs = now;
  goRight = !goRight;
  metronomeServo.write(goRight ? RIGHT_ANGLE : LEFT_ANGLE);
}

void setup() {
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);

  metronomeServo.attach(SERVO_PIN);
  metronomeServo.write(CENTER_ANGLE);

  Serial.begin(115200);
  delay(250);
  Serial.println(F("Servo metronome ready."));
  printHelp();
  setBpm(DEFAULT_BPM);
}

void loop() {
  handleSerial();
  handleButtons();
  updateSwing();
}
