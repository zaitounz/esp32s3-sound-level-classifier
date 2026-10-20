// sound_level_classifier.ino
// Real-time 4-level sound classifier for the Seeed XIAO ESP32-S3 Sense.
// Reads the onboard PDM mic, computes RMS over 20 ms windows, tracks the peak
// over 5 s, sets the LED blink rate, and sends an 8-byte binary telemetry packet.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <math.h>

I2SClass microphone;

const int      USER_LED         = 21;     // active-low
const uint32_t SAMPLE_RATE      = 16000;
const size_t   WINDOW_SAMPLES   = 320;    // 20 ms @ 16 kHz
const uint32_t UPDATE_PERIOD_MS = 5000;   // classify every 5 s
int16_t samples[WINDOW_SAMPLES];

// Thresholds tuned from measured RMS levels (see README)
const float THRESHOLD_1 = 0.015f;   // quiet       | normal speech
const float THRESHOLD_2 = 0.070f;   // normal      | loud speech
const float THRESHOLD_3 = 0.120f;   // loud speech | shout / clap

uint32_t lastToggleMs  = 0;
bool     ledState      = false;
uint32_t lastUpdateMs  = 0;
uint32_t blinkInterval = 0;
float    peakRms       = 0.0f;

// 8-byte packet, matches Python struct format '<BfHB'
struct __attribute__((packed)) TelemetryPacket {
  uint8_t  header = 0xAA;   // sync byte
  float    rms;             // IEEE 754, little-endian
  uint16_t interval;        // blink interval (ms)
  uint8_t  led;             // 0 = OFF, 1 = ON
};
static_assert(sizeof(TelemetryPacket) == 8, "TelemetryPacket must be 8 bytes");

void setUserLED(bool on) { digitalWrite(USER_LED, on ? LOW : HIGH); }

// Normalized RMS in [0, 1]; scale to float before squaring to avoid overflow
float calculateRMS(const int16_t *data, size_t count) {
  if (count == 0) return 0.0f;
  float sumSquares = 0.0f;
  for (size_t n = 0; n < count; n++) {
    float x = (float)data[n] / 32768.0f;
    sumSquares += x * x;
  }
  return sqrtf(sumSquares / (float)count);
}

// Threshold ladder: RMS -> blink interval (0 = LED off)
uint32_t intervalForRMS(float r) {
  if (r <= THRESHOLD_1) return 0;
  if (r <= THRESHOLD_2) return 500;
  if (r <= THRESHOLD_3) return 150;
  return 40;
}

// Non-blocking blink, called every loop
void updateBlinkLED(uint32_t intervalMs) {
  if (intervalMs == 0) {
    if (ledState) { ledState = false; setUserLED(false); }
    return;
  }
  uint32_t now = millis();
  if ((uint32_t)(now - lastToggleMs) >= intervalMs) {
    lastToggleMs = now;
    ledState = !ledState;
    setUserLED(ledState);
  }
}

void setup() {
  pinMode(USER_LED, OUTPUT);
  setUserLED(false);

  Serial.begin(115200);
  uint32_t started = millis();
  while (!Serial && (uint32_t)(millis() - started) < 3000) { delay(10); }

  microphone.setPinsPdmRx(42, 41);   // CLK, DATA
  if (!microphone.begin(I2S_MODE_PDM_RX, SAMPLE_RATE,
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    while (true) { delay(1000); }    // no text output: would corrupt the binary stream
  }
  lastUpdateMs = millis();
}

void loop() {
  size_t bytesRead = microphone.readBytes((char*)samples, sizeof(samples));
  if (bytesRead == 0) {
    setUserLED(false);
    while (true) { delay(1000); }
  }

  float rms = calculateRMS(samples, bytesRead / sizeof(int16_t));
  if (rms > peakRms) peakRms = rms;  // keep the loudest window so short claps are caught

  if ((uint32_t)(millis() - lastUpdateMs) >= UPDATE_PERIOD_MS) {
    blinkInterval = intervalForRMS(peakRms);

    TelemetryPacket packet;
    packet.rms      = peakRms;
    packet.interval = (uint16_t)blinkInterval;
    packet.led      = ledState ? 1 : 0;
    Serial.write((uint8_t*)&packet, sizeof(packet));

    lastUpdateMs = millis();
    peakRms = 0.0f;
  }

  updateBlinkLED(blinkInterval);
}
