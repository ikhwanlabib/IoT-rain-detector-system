/**
 * firebase_config.h
 *
 * User-editable configuration for the IoT Rain Detector System.
 *
 * Before uploading the sketch, fill in:
 *   - Your Wi-Fi SSID and password
 *   - Your Firebase project URL (e.g. https://your-project-default-rtdb.firebaseio.com/)
 *   - Your Firebase Web API key (Project Settings → General → Web API Key)
 *
 * Pin assignments can also be adjusted here to match your wiring.
 */

#ifndef FIREBASE_CONFIG_H
#define FIREBASE_CONFIG_H

// ---------------------------------------------------------------------------
// Wi-Fi credentials
// ---------------------------------------------------------------------------
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ---------------------------------------------------------------------------
// Firebase project settings
// ---------------------------------------------------------------------------
// Realtime Database URL (no trailing slash)
#define FIREBASE_HOST "https://YOUR_PROJECT_ID-default-rtdb.firebaseio.com"

// Web API key from Project Settings → General
#define FIREBASE_API_KEY "YOUR_FIREBASE_WEB_API_KEY"

// Firebase user credentials (can use anonymous auth or email/password auth).
// Leave blank if you set database rules to allow public read/write (not
// recommended for production).
#define FIREBASE_USER_EMAIL    ""
#define FIREBASE_USER_PASSWORD ""

// ---------------------------------------------------------------------------
// Hardware pin assignments (ESP32)
// ---------------------------------------------------------------------------

// Analog rain sensor output connected to an ADC-capable GPIO pin.
// Most ESP32 boards support ADC on GPIO 32–39.
#define PIN_RAIN_SENSOR   34   // Analog input  (ADC1_CH6)

// Digital output from the rain sensor module (LOW = rain detected).
// Not all modules expose this pin; set to -1 to disable.
#define PIN_RAIN_DIGITAL  35   // Digital input (active-LOW)

// LED indicators – drive through a current-limiting resistor (~220 Ω).
#define PIN_LED_NONE      25   // Green  – no rain
#define PIN_LED_LIGHT     26   // Yellow – light rain
#define PIN_LED_MODERATE  27   // Orange – moderate rain
#define PIN_LED_HEAVY     14   // Red    – heavy rain

// Buzzer (active buzzer or passive buzzer driven by PWM).
#define PIN_BUZZER        32   // Buzzer output

// ---------------------------------------------------------------------------
// Sampling & alert configuration
// ---------------------------------------------------------------------------

// How often (ms) to read the sensor and log to Firebase.
#define SAMPLE_INTERVAL_MS  5000UL   // 5 seconds

// ADC thresholds (0–4095 on 12-bit ESP32 ADC).
// Higher raw values typically mean less moisture (drier sensor).
// Adjust these thresholds to match your specific sensor.
#define THRESHOLD_NONE      3500     // Above this → no rain
#define THRESHOLD_LIGHT     2500     // Above this → light rain
#define THRESHOLD_MODERATE  1500     // Above this → moderate rain
                                     // At or below → heavy rain

// Number of buzzer beeps for a heavy-rain alert.
#define ALERT_BEEP_COUNT    3
#define ALERT_BEEP_ON_MS    200
#define ALERT_BEEP_OFF_MS   150

// Maximum history entries kept under /rain_history in Firebase.
// Older entries are pruned automatically.
#define MAX_HISTORY_ENTRIES 100

#endif // FIREBASE_CONFIG_H
