// rms_plotter.ino
// Streams normalized RMS of 20 ms microphone windows to the Arduino Serial Plotter.
// Used to measure noise floor and speech/clap levels before choosing thresholds.
#include <Arduino.h>
#include <ESP_I2S.h>
#include <math.h>

I2SClass microphone;

const int      USER_LED       = 21;     // active-low
const uint32_t SAMPLE_RATE    = 16000;
const size_t   WINDOW_SAMPLES = 320;    // 20 ms @ 16 kHz
int16_t samples[WINDOW_SAMPLES];

float calculateRMS(const int16_t *data, size_t count) {
  if (count == 0) return 0.0f;
  float sumSquares = 0.0f;
  for (size_t n = 0; n < count; n++) {
    float x = (float)data[n] / 32768.0f;
    sumSquares += x * x;
  }
  return sqrtf(sumSquares / (float)count);
}

void setup() {
  pinMode(USER_LED, OUTPUT);
  digitalWrite(USER_LED, HIGH);   // off

  Serial.begin(115200);
  uint32_t started = millis();
  while (!Serial && (uint32_t)(millis() - started) < 3000) { delay(10); }

  microphone.setPinsPdmRx(42, 41);   // CLK, DATA
  if (!microphone.begin(I2S_MODE_PDM_RX, SAMPLE_RATE,
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("ERROR: microphone initialization failed");
    while (true) { delay(1000); }
  }
}

void loop() {
  size_t bytesRead = microphone.readBytes((char*)samples, sizeof(samples));
  if (bytesRead == 0) {
    Serial.println("ERROR: audio read failed");
    while (true) { delay(1000); }
  }
  Serial.print("RMS:");
  Serial.println(calculateRMS(samples, bytesRead / sizeof(int16_t)), 4);
}
