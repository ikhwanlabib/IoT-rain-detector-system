/**
 * rain_detector.ino
 *
 * IoT Rain Detector System – ESP32
 * =================================
 * Features:
 *   - Analog rain sensor reading via ESP32 ADC (12-bit, 0-4095)
 *   - Rainfall intensity classification: None / Light / Moderate / Heavy
 *   - Visual indicators via four LEDs (green, yellow, orange, red)
 *   - Audible alert via buzzer for heavy rainfall
 *   - Wi-Fi connectivity with automatic reconnection
 *   - Real-time logging to Firebase Realtime Database:
 *       /rain_current  – latest sensor snapshot (always overwritten)
 *       /rain_history  – timestamped log entries (up to MAX_HISTORY_ENTRIES)
 *
 * Required libraries (install via Arduino Library Manager):
 *   - Firebase ESP Client  by Mobizt  (≥ 4.x)
 *     https://github.com/mobizt/Firebase-ESP-Client
 *   - NTPClient             by Fabrice Weinberg
 *   - WiFi                  (bundled with ESP32 Arduino core)
 *
 * Configuration:
 *   Edit firebase_config.h before uploading.
 *
 * Board settings (Arduino IDE):
 *   Board  : ESP32 Dev Module (or your specific ESP32 variant)
 *   Upload Speed: 115200
 *   CPU Frequency: 240 MHz
 */

#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Firebase_ESP_Client.h>

// Provide token generation and RTDB helper
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#include "firebase_config.h"

// ---------------------------------------------------------------------------
// Rainfall intensity levels
// ---------------------------------------------------------------------------
enum RainIntensity {
    RAIN_NONE     = 0,
    RAIN_LIGHT    = 1,
    RAIN_MODERATE = 2,
    RAIN_HEAVY    = 3
};

// Human-readable labels
const char* intensityLabel[] = {
    "None",
    "Light",
    "Moderate",
    "Heavy"
};

// ---------------------------------------------------------------------------
// Global objects
// ---------------------------------------------------------------------------
FirebaseData   fbdo;
FirebaseAuth   auth;
FirebaseConfig config;

WiFiUDP   ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // UTC

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static unsigned long lastSampleTime  = 0;
static bool          firebaseReady   = false;
static unsigned long historyIndex    = 0;   // monotonic key counter

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void     connectWiFi();
void     initFirebase();
void     readAndProcess();
RainIntensity classifyIntensity(int rawADC);
void     updateLEDs(RainIntensity intensity);
void     triggerBuzzerAlert(RainIntensity intensity);
void     logToFirebase(int rawADC, float voltage, RainIntensity intensity,
                       bool digitalDetected, const String& timestamp);
String   getTimestamp();
void     pruneHistory();

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("=== IoT Rain Detector System (ESP32) ===");

    // Configure GPIO pins
    pinMode(PIN_LED_NONE,     OUTPUT);
    pinMode(PIN_LED_LIGHT,    OUTPUT);
    pinMode(PIN_LED_MODERATE, OUTPUT);
    pinMode(PIN_LED_HEAVY,    OUTPUT);
    pinMode(PIN_BUZZER,       OUTPUT);

    if (PIN_RAIN_DIGITAL >= 0) {
        pinMode(PIN_RAIN_DIGITAL, INPUT);
    }

    // ADC resolution: 12-bit (0–4095)
    analogReadResolution(12);
    // Use the internal 11 dB attenuation to allow 0–3.3 V range
    analogSetAttenuation(ADC_11db);

    // Startup LED test – cycle all LEDs once
    Serial.println("LED self-test...");
    const int testPins[] = {PIN_LED_NONE, PIN_LED_LIGHT, PIN_LED_MODERATE, PIN_LED_HEAVY};
    for (int pin : testPins) {
        digitalWrite(pin, HIGH);
        delay(200);
        digitalWrite(pin, LOW);
    }

    connectWiFi();
    timeClient.begin();

    initFirebase();

    Serial.println("Setup complete. Starting monitoring loop.");
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    // Keep NTP updated
    timeClient.update();

    // Reconnect Wi-Fi if needed
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi lost – reconnecting...");
        connectWiFi();
    }

    // Refresh Firebase token when it is about to expire
    if (Firebase.isTokenExpired()) {
        Firebase.refreshToken(&config);
    }

    // Sample at configured interval
    unsigned long now = millis();
    if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
        lastSampleTime = now;
        readAndProcess();
    }
}

// ---------------------------------------------------------------------------
// connectWiFi() – blocks until connected or times out (60 s)
// ---------------------------------------------------------------------------
void connectWiFi() {
    Serial.printf("Connecting to Wi-Fi: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 60000UL) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("Wi-Fi connected. IP: %s\n",
                      WiFi.localIP().toString().c_str());
    } else {
        Serial.println("Wi-Fi connection failed. Continuing offline.");
    }
}

// ---------------------------------------------------------------------------
// initFirebase() – configure and authenticate with Firebase
// ---------------------------------------------------------------------------
void initFirebase() {
    config.api_key           = FIREBASE_API_KEY;
    config.database_url      = FIREBASE_HOST;
    config.token_status_callback = tokenStatusCallback; // from TokenHelper.h

    // Use email/password authentication when credentials are provided;
    // otherwise fall back to anonymous sign-in.
    if (strlen(FIREBASE_USER_EMAIL) > 0) {
        auth.user.email    = FIREBASE_USER_EMAIL;
        auth.user.password = FIREBASE_USER_PASSWORD;
    }

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // Wait for authentication (up to 10 s)
    Serial.print("Authenticating with Firebase");
    unsigned long start = millis();
    while (!Firebase.ready() && millis() - start < 10000UL) {
        delay(500);
        Serial.print('.');
    }
    Serial.println();

    if (Firebase.ready()) {
        firebaseReady = true;
        Serial.println("Firebase ready.");
    } else {
        Serial.println("Firebase not ready – logging will be disabled.");
    }
}

// ---------------------------------------------------------------------------
// readAndProcess() – read sensor, update outputs, log to cloud
// ---------------------------------------------------------------------------
void readAndProcess() {
    // --- Analog reading ---
    int rawADC = analogRead(PIN_RAIN_SENSOR);
    // Convert to voltage (3.3 V reference, 12-bit ADC)
    float voltage = rawADC * (3.3f / 4095.0f);

    // --- Digital reading (if pin configured) ---
    bool digitalDetected = false;
    if (PIN_RAIN_DIGITAL >= 0) {
        digitalDetected = (digitalRead(PIN_RAIN_DIGITAL) == LOW);
    }

    // --- Classify intensity ---
    RainIntensity intensity = classifyIntensity(rawADC);

    // --- Serial output ---
    Serial.printf("[%s] ADC: %4d  Voltage: %.2fV  Intensity: %-8s  Digital: %s\n",
                  getTimestamp().c_str(),
                  rawADC, voltage,
                  intensityLabel[intensity],
                  digitalDetected ? "YES" : "no");

    // --- Update visual indicators ---
    updateLEDs(intensity);

    // --- Buzzer alert ---
    triggerBuzzerAlert(intensity);

    // --- Cloud logging ---
    if (firebaseReady && WiFi.status() == WL_CONNECTED) {
        logToFirebase(rawADC, voltage, intensity, digitalDetected, getTimestamp());
    }
}

// ---------------------------------------------------------------------------
// classifyIntensity() – map raw ADC value to RainIntensity enum
// ---------------------------------------------------------------------------
RainIntensity classifyIntensity(int rawADC) {
    if (rawADC > THRESHOLD_NONE) {
        return RAIN_NONE;
    } else if (rawADC > THRESHOLD_LIGHT) {
        return RAIN_LIGHT;
    } else if (rawADC > THRESHOLD_MODERATE) {
        return RAIN_MODERATE;
    } else {
        return RAIN_HEAVY;
    }
}

// ---------------------------------------------------------------------------
// updateLEDs() – turn on exactly the LED matching current intensity
// ---------------------------------------------------------------------------
void updateLEDs(RainIntensity intensity) {
    digitalWrite(PIN_LED_NONE,     intensity == RAIN_NONE     ? HIGH : LOW);
    digitalWrite(PIN_LED_LIGHT,    intensity == RAIN_LIGHT    ? HIGH : LOW);
    digitalWrite(PIN_LED_MODERATE, intensity == RAIN_MODERATE ? HIGH : LOW);
    digitalWrite(PIN_LED_HEAVY,    intensity == RAIN_HEAVY    ? HIGH : LOW);
}

// ---------------------------------------------------------------------------
// triggerBuzzerAlert() – beep buzzer only on heavy rain
// ---------------------------------------------------------------------------
void triggerBuzzerAlert(RainIntensity intensity) {
    if (intensity != RAIN_HEAVY) {
        digitalWrite(PIN_BUZZER, LOW);
        return;
    }

    for (int i = 0; i < ALERT_BEEP_COUNT; i++) {
        digitalWrite(PIN_BUZZER, HIGH);
        delay(ALERT_BEEP_ON_MS);
        digitalWrite(PIN_BUZZER, LOW);
        if (i < ALERT_BEEP_COUNT - 1) {
            delay(ALERT_BEEP_OFF_MS);
        }
    }
}

// ---------------------------------------------------------------------------
// logToFirebase() – write current snapshot and append to history
// ---------------------------------------------------------------------------
void logToFirebase(int rawADC, float voltage, RainIntensity intensity,
                   bool digitalDetected, const String& timestamp) {
    // --- /rain_current (always overwrite) ---
    FirebaseJson currentJson;
    currentJson.set("raw_adc",          rawADC);
    currentJson.set("voltage",          voltage);
    currentJson.set("intensity_code",   (int)intensity);
    currentJson.set("intensity_label",  intensityLabel[intensity]);
    currentJson.set("digital_detected", digitalDetected);
    currentJson.set("timestamp",        timestamp);

    if (!Firebase.RTDB.setJSON(&fbdo, "/rain_current", &currentJson)) {
        Serial.printf("Firebase /rain_current error: %s\n",
                      fbdo.errorReason().c_str());
    }

    // --- /rain_history/<index> (append) ---
    String histPath = "/rain_history/" + String(historyIndex++);

    FirebaseJson histJson;
    histJson.set("raw_adc",          rawADC);
    histJson.set("voltage",          voltage);
    histJson.set("intensity_code",   (int)intensity);
    histJson.set("intensity_label",  intensityLabel[intensity]);
    histJson.set("digital_detected", digitalDetected);
    histJson.set("timestamp",        timestamp);

    if (!Firebase.RTDB.setJSON(&fbdo, histPath.c_str(), &histJson)) {
        Serial.printf("Firebase %s error: %s\n",
                      histPath.c_str(), fbdo.errorReason().c_str());
    }

    // Prune old entries once history grows too large
    if (historyIndex > MAX_HISTORY_ENTRIES) {
        pruneHistory();
    }
}

// ---------------------------------------------------------------------------
// pruneHistory() – remove oldest history entries beyond MAX_HISTORY_ENTRIES
// ---------------------------------------------------------------------------
void pruneHistory() {
    if (historyIndex <= (unsigned long)MAX_HISTORY_ENTRIES) {
        return;
    }

    unsigned long oldest = historyIndex - MAX_HISTORY_ENTRIES;
    String prunePath = "/rain_history/" + String(oldest - 1);

    if (!Firebase.RTDB.deleteNode(&fbdo, prunePath.c_str())) {
        Serial.printf("Firebase prune error: %s\n",
                      fbdo.errorReason().c_str());
    }
}

// ---------------------------------------------------------------------------
// getTimestamp() – return ISO-8601 UTC string from NTP client
// ---------------------------------------------------------------------------
String getTimestamp() {
    time_t epochTime = (time_t)timeClient.getEpochTime();

    struct tm timeInfo;
    gmtime_r(&epochTime, &timeInfo);

    char buf[25];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeInfo);
    return String(buf);
}
