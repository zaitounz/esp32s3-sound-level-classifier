# ESP32-S3 Real-Time Sound Level Classifier

Firmware for the **Seeed XIAO ESP32-S3 Sense** that listens through the onboard PDM microphone, measures loudness in real time, sorts it into 4 levels, and reports each result as an 8-byte binary packet decoded by a Python monitor on the host.

Built as part of the ELEC 3000 (Engineering Design) labs at the University of Windsor, Fall 2026.

## How it works

```
PDM mic (16 kHz, 16-bit) ──► 20 ms windows (320 samples) ──► normalized RMS
        ──► peak RMS over 5 s ──► threshold ladder ──► LED blink rate + 8-byte telemetry packet
```

| Level | Peak RMS range | LED behaviour | Typical sound |
|---|---|---|---|
| 1 | ≤ 0.015 | Off | Background / quiet room |
| 2 | 0.015 – 0.070 | Slow blink (500 ms) | Normal speech |
| 3 | 0.070 – 0.120 | Fast blink (150 ms) | Loud speech |
| 4 | > 0.120 | Strobe (40 ms) | Shouting, clapping |

- **RMS:** samples are scaled to [-1, 1] *before* squaring, to avoid int16 overflow. The mean isn't usable as a loudness measure because audio averages to roughly 0.
- **Peak tracking:** the loudest 20 ms window in each 5 s period is used, so short claps aren't missed.
- **Non-blocking LED:** the blink timer is driven by `millis()` every loop, so audio capture never stalls.

## Measured RMS levels (onboard mic)

| Condition | Observed RMS |
|---|---|
| Quiet room | 0.000 – 0.013 |
| Distant speech (~1 m) | 0.007 – 0.038 |
| Near speech (15–20 cm) | 0.017 – 0.09 |
| Shouting (close) | 0.07 – 0.40 |
| Clapping | 0.013 – 0.21 |

Distant speech overlaps with the quiet range, which shows why loudness alone can't recognize *words*. That limitation is what motivates moving to a keyword-spotting model.

## Telemetry packet (8 bytes, little-endian, `<BfHB`)

| Byte | Field | Type |
|---|---|---|
| 0 | Sync header `0xAA` | uint8 |
| 1–4 | Peak RMS | float32 |
| 5–6 | Blink interval (ms) | uint16 |
| 7 | LED state (0/1) | uint8 |

Example: `pack_telemetry(0.0421, 150, "ON")` → `aa 0d 71 2c 3d 96 00 01`

The decoder resyncs on `0xAA`. That byte can also appear inside the float, so each frame is also checked against the valid interval set, the LED byte and the RMS range before it's accepted.

## Repository layout

```
firmware/sound_level_classifier/  # main firmware: classification + binary telemetry
firmware/rms_plotter/             # streams live RMS to the Arduino Serial Plotter
tools/sound_led_monitor.py        # live decoder (pyserial)
tools/telemetry_formatter.py      # offline pack/unpack test
```

## Running it

1. Arduino IDE 2.x → install **esp32 by Espressif (3.x)** → board **XIAO_ESP32S3**, set *USB CDC On Boot* to Enabled.
2. Upload `firmware/sound_level_classifier`.
3. Close the Serial Monitor, set `PORT` in `tools/sound_led_monitor.py`, then:
   ```
   pip install pyserial
   python tools/sound_led_monitor.py
   ```

**Hardware:** Seeed XIAO ESP32-S3 Sense (PDM mic on GPIO 42 = CLK, GPIO 41 = DATA; active-low user LED on GPIO 21).
