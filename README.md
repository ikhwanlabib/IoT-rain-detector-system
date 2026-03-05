# IoT Rain Detector System (ESP32)

An enhanced IoT rain detection system using **ESP32** that:

- Reads rainfall intensity from an **analog rain sensor**
- Classifies rainfall into four levels: **None / Light / Moderate / Heavy**
- Drives **four LED indicators** (green → yellow → orange → red)
- Triggers a **buzzer alert** during heavy rainfall
- Connects to **Wi-Fi** and logs every reading to **Firebase Realtime Database**
  for real-time monitoring and historical analysis

---

## Hardware Requirements

| Component | Description |
|-----------|-------------|
| ESP32 Dev Module | Main microcontroller (3.3 V logic, built-in Wi-Fi) |
| Analog Rain Sensor | Any module with both AO (analog) and DO (digital) outputs |
| Green LED + 220 Ω resistor | "No rain" indicator |
| Yellow LED + 220 Ω resistor | "Light rain" indicator |
| Orange LED + 220 Ω resistor | "Moderate rain" indicator |
| Red LED + 220 Ω resistor | "Heavy rain" indicator |
| Active Buzzer | Alert for heavy rainfall |
| Breadboard + jumper wires | Prototyping |
| Micro-USB cable | Programming and power |

---

## Wiring Diagram

```
Rain Sensor Module          ESP32
──────────────────          ──────
AO  ──────────────────────► GPIO 34  (analog input)
DO  ──────────────────────► GPIO 35  (digital input, optional)
VCC ──────────────────────► 3V3
GND ──────────────────────► GND

Green  LED (+) ──[220Ω]──► GPIO 25
Yellow LED (+) ──[220Ω]──► GPIO 26
Orange LED (+) ──[220Ω]──► GPIO 27
Red    LED (+) ──[220Ω]──► GPIO 14
All LED (-)    ──────────► GND

Buzzer (+) ──────────────► GPIO 32
Buzzer (-) ──────────────► GND
```

> **Note:** GPIO 34 and 35 are input-only pins on the ESP32 — they cannot be
> used as outputs, which makes them ideal for sensor inputs.

---

## Software Setup

### 1. Install Arduino IDE & ESP32 Board Support

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software).
2. Add the ESP32 board URL in **File → Preferences → Additional Boards Manager URLs**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Install **esp32 by Espressif Systems** via **Tools → Board → Boards Manager**.

### 2. Install Required Libraries

Open **Tools → Manage Libraries** and install:

| Library | Author | Version |
|---------|--------|---------|
| Firebase ESP Client | Mobizt | ≥ 4.x |
| NTPClient | Fabrice Weinberg | ≥ 3.x |

### 3. Configure Firebase

1. Go to the [Firebase Console](https://console.firebase.google.com/) and create a project.
2. Enable **Realtime Database** (start in test mode or set up authentication rules).
3. Note down:
   - **Database URL** – shown in the Realtime Database dashboard
     (e.g. `https://your-project-default-rtdb.firebaseio.com`)
   - **Web API Key** – found in **Project Settings → General → Web API Key**

### 4. Edit `firebase_config.h`

Open `rain_detector/firebase_config.h` and fill in your credentials:

```cpp
#define WIFI_SSID        "Your_WiFi_Name"
#define WIFI_PASSWORD    "Your_WiFi_Password"

#define FIREBASE_HOST    "https://your-project-default-rtdb.firebaseio.com"
#define FIREBASE_API_KEY "Your_Web_API_Key"
```

Adjust pin assignments and ADC thresholds if your hardware differs.

### 5. Upload the Sketch

1. Open `rain_detector/rain_detector.ino` in Arduino IDE.
2. Select **Tools → Board → ESP32 Dev Module** (or your specific variant).
3. Select the correct **Port**.
4. Click **Upload**.

---

## Firebase Data Structure

```
(root)
├── rain_current/                  ← always holds the latest reading
│   ├── raw_adc          : 2104
│   ├── voltage          : 1.69
│   ├── intensity_code   : 2
│   ├── intensity_label  : "Moderate"
│   ├── digital_detected : false
│   └── timestamp        : "2025-06-01T08:30:00Z"
│
└── rain_history/
    ├── 0/  { raw_adc, voltage, intensity_code, intensity_label, digital_detected, timestamp }
    ├── 1/  { ... }
    └── ...  (up to MAX_HISTORY_ENTRIES entries)
```

---

## Rainfall Intensity Classification

| Intensity | Raw ADC (12-bit) | Indicator | Buzzer |
|-----------|-----------------|-----------|--------|
| None | > 3500 | Green LED | Off |
| Light | 2501 – 3500 | Yellow LED | Off |
| Moderate | 1501 – 2500 | Orange LED | Off |
| Heavy | ≤ 1500 | Red LED | 3 beeps |

> Thresholds are configurable via `THRESHOLD_NONE`, `THRESHOLD_LIGHT`, and
> `THRESHOLD_MODERATE` in `firebase_config.h`.

---

## Serial Monitor Output

Set baud rate to **115200**. Example output:

```
=== IoT Rain Detector System (ESP32) ===
LED self-test...
Connecting to Wi-Fi: MyNetwork
.....
Wi-Fi connected. IP: 192.168.1.42
Authenticating with Firebase...
Firebase ready.
Setup complete. Starting monitoring loop.
[2025-06-01T08:30:00Z] ADC: 3812  Voltage: 3.07V  Intensity: None      Digital: no
[2025-06-01T08:30:05Z] ADC: 2241  Voltage: 1.81V  Intensity: Moderate  Digital: YES
[2025-06-01T08:30:10Z] ADC: 1102  Voltage: 0.89V  Intensity: Heavy     Digital: YES
```

---

## Configuration Reference (`firebase_config.h`)

| Constant | Default | Description |
|----------|---------|-------------|
| `WIFI_SSID` | `"YOUR_WIFI_SSID"` | Wi-Fi network name |
| `WIFI_PASSWORD` | `"YOUR_WIFI_PASSWORD"` | Wi-Fi password |
| `FIREBASE_HOST` | — | Firebase Realtime Database URL |
| `FIREBASE_API_KEY` | — | Firebase Web API key |
| `FIREBASE_USER_EMAIL` | `""` | Firebase user email (optional) |
| `FIREBASE_USER_PASSWORD` | `""` | Firebase user password (optional) |
| `PIN_RAIN_SENSOR` | `34` | ADC pin for analog rain sensor |
| `PIN_RAIN_DIGITAL` | `35` | Digital pin for DO output (`-1` to disable) |
| `PIN_LED_NONE` | `25` | Green LED pin |
| `PIN_LED_LIGHT` | `26` | Yellow LED pin |
| `PIN_LED_MODERATE` | `27` | Orange LED pin |
| `PIN_LED_HEAVY` | `14` | Red LED pin |
| `PIN_BUZZER` | `32` | Buzzer pin |
| `SAMPLE_INTERVAL_MS` | `5000` | Sampling interval in milliseconds |
| `THRESHOLD_NONE` | `3500` | ADC threshold – no rain |
| `THRESHOLD_LIGHT` | `2500` | ADC threshold – light rain |
| `THRESHOLD_MODERATE` | `1500` | ADC threshold – moderate rain |
| `ALERT_BEEP_COUNT` | `3` | Number of buzzer beeps for heavy rain |
| `MAX_HISTORY_ENTRIES` | `100` | Maximum Firebase history log entries |

---

## Project Structure

```
IoT-rain-detector-system-/
└── rain_detector/
    ├── rain_detector.ino   ← Main ESP32 sketch
    └── firebase_config.h   ← User configuration (Wi-Fi, Firebase, pins)
```

---

## License

This project is released under the [MIT License](LICENSE).
