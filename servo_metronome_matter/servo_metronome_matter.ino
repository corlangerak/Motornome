/*
  Motornome — Dual-Servo BLE Metronome
  Target: Arduino Nano Matter (Silicon Labs EFR32MG24)

  Libraries required (install via Arduino Library Manager):
    • ArduinoBLE
    • Servo  (bundled with the arduino:silabs board package)

  Wiring:
    Servo 1 signal  → D9
    Servo 2 signal  → D10
    Servo power     → external 5 V supply
    Common GND      → Nano Matter GND + servo supply GND

  BLE Protocol (all values little-endian):
    Service UUID : 12340000-0000-0000-0000-000000000000
    Char 1 (S1)  : 12340001-0000-0000-0000-000000000000  Write|WriteWithoutResponse
    Char 2 (S2)  : 12340002-0000-0000-0000-000000000000  Write|WriteWithoutResponse
    Status       : 12340003-0000-0000-0000-000000000000  Read|Notify

  Command bytes:
    0x01 + float32 bpm   → SET_BPM   (5 bytes)
    0x02                 → PLAY      (1 byte)
    0x03                 → STOP      (1 byte)
    0x04 + uint16 N
         + N×{uint32 timeMs, float32 bpm}  → LOAD_SEQ  (3 + N×8 bytes, max N=62)
    0x05                 → PLAY_SEQ  (1 byte)

  Status notification (8 bytes): [float32 bpm1, float32 bpm2]
*/

#include <ArduinoBLE.h>
#include <Servo.h>

// ── Pins ──────────────────────────────────────────────────────────────────────
static constexpr uint8_t SERVO1_PIN = 9;
static constexpr uint8_t SERVO2_PIN = 10;

// ── Motion constants ──────────────────────────────────────────────────────────
static constexpr int   CENTER_ANGLE   = 90;    // neutral servo position
static constexpr int   MAX_SWING_DEG  = 40;    // max ±degrees from centre
static constexpr float MIN_LIVE_BPM   = 10.0f; // below this, servo holds centre
static constexpr float MAX_BPM        = 1000.0f;

// Servo speed: 0.08 s per 60° → 1.333 ms per degree
static constexpr float MS_PER_DEGREE  = 80.0f / 60.0f;

// ── BLE UUIDs ─────────────────────────────────────────────────────────────────
static const char* SVC_UUID  = "12340000-0000-0000-0000-000000000000";
static const char* CH1_UUID  = "12340001-0000-0000-0000-000000000000";
static const char* CH2_UUID  = "12340002-0000-0000-0000-000000000000";
static const char* STAT_UUID = "12340003-0000-0000-0000-000000000000";

// ── Sequence storage ──────────────────────────────────────────────────────────
// 62 keyframes × 8 bytes = 496 bytes + 3 byte header = 499 bytes < 512 byte BLE limit
static constexpr uint16_t MAX_KF = 62;

struct Keyframe {
  uint32_t timeMs;
  float    bpm;
};

// ── Per-servo state ───────────────────────────────────────────────────────────
struct ServoState {
  Servo         servo;
  float         liveBpm    = 120.0f;
  bool          running    = false;
  bool          goRight    = true;
  unsigned long lastStepMs = 0;
  Keyframe      seq[MAX_KF];
  uint16_t      seqLen     = 0;
  bool          playingSeq = false;
  unsigned long seqStartMs = 0;
};

static ServoState s1, s2;

// ── BLE objects ───────────────────────────────────────────────────────────────
BLEService        bleService(SVC_UUID);
BLECharacteristic bleCh1(CH1_UUID,  BLEWrite | BLEWriteWithoutResponse, 512);
BLECharacteristic bleCh2(CH2_UUID,  BLEWrite | BLEWriteWithoutResponse, 512);
BLECharacteristic bleStat(STAT_UUID, BLERead | BLENotify, 8);

// ── Helpers ───────────────────────────────────────────────────────────────────

// Returns the swing angle (degrees) the servo can reliably complete in one
// half-beat at the given BPM, capped at MAX_SWING_DEG.
static int swingAngle(float bpm) {
  float halfMs = 30000.0f / bpm;
  float maxDeg = halfMs / MS_PER_DEGREE;
  return (int)min((float)MAX_SWING_DEG, maxDeg);
}

// Linear interpolation between sequence keyframes.
static float interpolateBpm(const ServoState& s, uint32_t elapsedMs) {
  if (s.seqLen == 0)                           return s.liveBpm;
  if (elapsedMs <= s.seq[0].timeMs)            return s.seq[0].bpm;
  for (uint16_t i = 1; i < s.seqLen; i++) {
    if (elapsedMs <= s.seq[i].timeMs) {
      float t = (float)(elapsedMs          - s.seq[i - 1].timeMs) /
                (float)(s.seq[i].timeMs    - s.seq[i - 1].timeMs);
      return s.seq[i - 1].bpm + t * (s.seq[i].bpm - s.seq[i - 1].bpm);
    }
  }
  return s.seq[s.seqLen - 1].bpm;
}

// ── Command handler ───────────────────────────────────────────────────────────
static void handleCommand(ServoState& s, const uint8_t* d, int len) {
  if (len < 1) return;

  switch (d[0]) {

    case 0x01: // SET_BPM
      if (len >= 5) {
        float bpm;
        memcpy(&bpm, d + 1, 4);
        s.liveBpm    = constrain(bpm, 1.0f, MAX_BPM);
        s.playingSeq = false;
      }
      break;

    case 0x02: // PLAY
      s.running    = true;
      s.goRight    = true;
      s.lastStepMs = millis();
      s.playingSeq = false;
      break;

    case 0x03: // STOP
      s.running    = false;
      s.playingSeq = false;
      s.servo.write(CENTER_ANGLE);
      break;

    case 0x04: // LOAD_SEQ
      if (len >= 3) {
        uint16_t n;
        memcpy(&n, d + 1, 2);
        n        = min(n, MAX_KF);
        s.seqLen = 0;
        for (uint16_t i = 0; i < n; i++) {
          int off = 3 + i * 8;
          if (off + 8 > len) break;
          memcpy(&s.seq[i].timeMs, d + off,     4);
          memcpy(&s.seq[i].bpm,   d + off + 4, 4);
          s.seqLen++;
        }
      }
      break;

    case 0x05: // PLAY_SEQ
      if (s.seqLen > 0) {
        s.running    = true;
        s.playingSeq = true;
        s.seqStartMs = millis();
        s.goRight    = true;
        s.lastStepMs = millis();
      }
      break;
  }
}

// ── Servo update (called every loop iteration) ────────────────────────────────
static void updateServo(ServoState& s) {
  if (!s.running) return;

  unsigned long now      = millis();
  float         activeBpm = s.liveBpm;

  if (s.playingSeq && s.seqLen > 0) {
    uint32_t elapsed = (uint32_t)(now - s.seqStartMs);
    if (elapsed >= s.seq[s.seqLen - 1].timeMs) {
      // Sequence finished
      s.running    = false;
      s.playingSeq = false;
      s.servo.write(CENTER_ANGLE);
      return;
    }
    activeBpm = interpolateBpm(s, elapsed);
  }

  if (activeBpm < MIN_LIVE_BPM) {
    s.servo.write(CENTER_ANGLE);
    return;
  }

  unsigned long halfBeat = (unsigned long)(30000.0f / activeBpm);
  if (now - s.lastStepMs >= halfBeat) {
    s.lastStepMs = now;
    s.goRight    = !s.goRight;
    int angle    = swingAngle(activeBpm);
    s.servo.write(CENTER_ANGLE + (s.goRight ? angle : -angle));
  }
}

// ── Status notification ───────────────────────────────────────────────────────
static void pushStatus() {
  uint8_t buf[8];
  memcpy(buf,     &s1.liveBpm, 4);
  memcpy(buf + 4, &s2.liveBpm, 4);
  bleStat.writeValue(buf, 8);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  s1.servo.attach(SERVO1_PIN);
  s2.servo.attach(SERVO2_PIN);
  s1.servo.write(CENTER_ANGLE);
  s2.servo.write(CENTER_ANGLE);

  if (!BLE.begin()) {
    Serial.println(F("BLE init failed"));
    while (true) {}
  }

  BLE.setLocalName("Motornome");
  BLE.setAdvertisedService(bleService);
  bleService.addCharacteristic(bleCh1);
  bleService.addCharacteristic(bleCh2);
  bleService.addCharacteristic(bleStat);
  BLE.addService(bleService);
  BLE.advertise();

  Serial.println(F("Motornome ready — waiting for BLE connection"));
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  BLEDevice central = BLE.central();
  if (!central) return;

  Serial.print(F("Connected: "));
  Serial.println(central.address());

  while (central.connected()) {
    if (bleCh1.written()) {
      handleCommand(s1, bleCh1.value(), bleCh1.valueLength());
      pushStatus();
    }
    if (bleCh2.written()) {
      handleCommand(s2, bleCh2.value(), bleCh2.valueLength());
      pushStatus();
    }
    updateServo(s1);
    updateServo(s2);
  }

  Serial.println(F("Disconnected — servos centred"));
  s1.running = false;
  s2.running = false;
  s1.servo.write(CENTER_ANGLE);
  s2.servo.write(CENTER_ANGLE);
}
