// StampFly EEG → MQTT drone control (subscribe side).
//
// Receives control commands published by the GitHub Pages web UI over an MQTT
// broker (public HiveMQ). The browser publishes JSON {"v":0..10,"arm":bool,"ts":ms}
// to <TOPIC_BASE>/control; this firmware maps v → a small yaw-differential motor
// output and drives the motors ONLY while armed and receiving fresh commands.
//
// Self-contained (own PWM + LED + MQTT) so it builds under a single-file
// build_src_filter without pulling the full flight stack. Motor PWM mirrors
// flight_control.cpp exactly (150 kHz / 8-bit, pins 5/42/10/41, duty = 255*d).
//
// Build/flash:  pio run -e eeg_mqtt -t upload
//
// SAFETY: props OFF for first tests. Motors spin only when armed AND a command
// arrived within DATA_TIMEOUT_MS, capped at MAX_DUTY, auto-stopped after RUN_LIMIT_MS.

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>

// ---- WiFi (from .env via scripts/load_env.py) -----------------------------
#ifndef WIFI_SSID
#define WIFI_SSID "default_ssid"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "default_password"
#endif
static const char* WIFI_SSID_S = WIFI_SSID;
static const char* WIFI_PASS_S = WIFI_PASSWORD;

// ---- MQTT -----------------------------------------------------------------
static const char* MQTT_BROKER = "broker.hivemq.com";
static const int MQTT_PORT = 1883;
// Must match the web UI's "Topic" field (default there is "stampfly/demo").
#define TOPIC_BASE "stampfly/demo"
static const char* TOPIC_CONTROL = TOPIC_BASE "/control";
static const char* TOPIC_TELEM = TOPIC_BASE "/telemetry";
static const char* MQTT_CLIENT_ID = "StampFly-S3-eeg";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ---- Motor PWM (identical to flight_control.cpp) --------------------------
static const int pwmFrontLeft = 5, pwmFrontRight = 42, pwmRearLeft = 10, pwmRearRight = 41;
static const int FL = 0, FR = 1, RL = 2, RR = 3;
static const int PWM_FREQ = 150000;
static const int PWM_RES = 8;

static void init_pwm() {
  ledcSetup(FL, PWM_FREQ, PWM_RES);
  ledcSetup(FR, PWM_FREQ, PWM_RES);
  ledcSetup(RL, PWM_FREQ, PWM_RES);
  ledcSetup(RR, PWM_FREQ, PWM_RES);
  ledcAttachPin(pwmFrontLeft, FL);
  ledcAttachPin(pwmFrontRight, FR);
  ledcAttachPin(pwmRearLeft, RL);
  ledcAttachPin(pwmRearRight, RR);
}
static void set_duty(int ch, float duty) {
  if (duty < 0.0f) duty = 0.0f;
  if (duty > 1.0f) duty = 1.0f;
  ledcWrite(ch, (uint32_t)(255.0f * duty));
}
static void motors_off() {
  set_duty(FL, 0.0f);
  set_duty(FR, 0.0f);
  set_duty(RL, 0.0f);
  set_duty(RR, 0.0f);
}

// ---- Status LED (single WS2812 on ESP pin) --------------------------------
#define LED_PIN 21
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];
static void led(uint32_t rgb) {
  leds[0] = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
  FastLED.show();
}

// ---- Safety limits --------------------------------------------------------
static const float BASE_DUTY = 0.15f;   // gentle hover-ish baseline (props off first!)
static const float YAW_DIFF_MAX = 0.10f; // max per-motor differential
static const float MAX_DUTY = 0.30f;     // hard cap per motor
static const unsigned long DATA_TIMEOUT_MS = 3000;  // stop if no command
static const unsigned long RUN_LIMIT_MS = 15000;    // max continuous armed run

// ---- Control state --------------------------------------------------------
static volatile float g_v = 0.0f;      // last commanded value 0..10
static volatile bool g_armed = false;  // latched from last command
static unsigned long g_last_cmd = 0;   // millis of last valid command
static unsigned long g_run_start = 0;  // when the current armed run began
static bool g_running = false;

// v(0..10) → yaw differential + LED color. Mirrors eeg_rotation_control buckets.
static float yaw_from_v(float v) {
  if (v < 1.0f) { led(0xFF00FF); return -YAW_DIFF_MAX; }        // magenta: strong left
  if (v < 2.0f) { led(0x0000FF); return -YAW_DIFF_MAX * 0.5f; } // blue: soft left
  if (v < 3.0f) { led(0x00FF00); return 0.0f; }                 // green: straight
  if (v < 4.0f) { led(0xFFFF00); return YAW_DIFF_MAX * 0.5f; }  // yellow: soft right
  led(0xFF0000);                                                // red: strong right
  return YAW_DIFF_MAX;
}

static void drive_from_command() {
  float yaw = yaw_from_v(g_v);
  // Diagonal pairs differ to produce yaw; clamp each to the hard cap.
  set_duty(FL, min(BASE_DUTY + yaw, MAX_DUTY));
  set_duty(FR, min(BASE_DUTY - yaw, MAX_DUTY));
  set_duty(RL, min(BASE_DUTY + yaw, MAX_DUTY));
  set_duty(RR, min(BASE_DUTY - yaw, MAX_DUTY));
}

static void onMessage(char* topic, byte* payload, unsigned int len) {
  (void)topic;
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, payload, len)) return; // ignore malformed
  float v = doc["v"] | 0.0f;
  bool arm = doc["arm"] | false;
  if (v < 0.0f || v > 10.0f) return; // range guard
  g_v = v;
  g_armed = arm;
  g_last_cmd = millis();
}

static void connectWiFi() {
  Serial.printf("WiFi: connecting to %s\r\n", WIFI_SSID_S);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID_S, WIFI_PASS_S);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 40) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("\r\nWiFi OK, IP=%s\r\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("\r\nWiFi FAILED");
}

static void reconnectMQTT() {
  if (mqtt.connected()) return;
  Serial.printf("MQTT: connecting %s:%d ...\r\n", MQTT_BROKER, MQTT_PORT);
  if (mqtt.connect(MQTT_CLIENT_ID)) {
    mqtt.subscribe(TOPIC_CONTROL);
    Serial.printf("MQTT OK, subscribed %s\r\n", TOPIC_CONTROL);
    led(0x001133);
  } else {
    Serial.printf("MQTT failed rc=%d\r\n", mqtt.state());
    led(0x110000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\r\n=== StampFly EEG MQTT Drone Control ===");

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(40);
  led(0x111111);

  init_pwm();
  motors_off(); // ensure stopped at boot

  connectWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMessage);
  reconnectMQTT();

  Serial.printf("Control topic: %s\r\n", TOPIC_CONTROL);
  Serial.println("Motors spin only while ARMED + fresh command. Props off for first test!");
}

void loop() {
  static unsigned long lastReconnect = 0, lastTelem = 0;

  if (!mqtt.connected() && millis() - lastReconnect > 3000) {
    lastReconnect = millis();
    reconnectMQTT();
  }
  mqtt.loop();

  const unsigned long now = millis();
  const bool fresh = (now - g_last_cmd) < DATA_TIMEOUT_MS;

  if (g_armed && fresh) {
    if (!g_running) { g_running = true; g_run_start = now; }
    if (now - g_run_start > RUN_LIMIT_MS) {
      motors_off(); // 15s safety cutoff — needs re-arm (disarm+arm) to resume
      led(0xFF6600);
    } else {
      drive_from_command();
    }
  } else {
    if (g_running) Serial.println("Disarmed / no data → motors off");
    g_running = false;
    motors_off();
    if (!g_armed) led(0x111111);      // idle
    else if (!fresh) led(0x331100);   // armed but stale data
  }

  // Telemetry ~1 Hz
  if (mqtt.connected() && now - lastTelem > 1000) {
    lastTelem = now;
    StaticJsonDocument<128> t;
    t["v"] = g_v;
    t["armed"] = g_armed;
    t["running"] = g_running;
    t["uptime"] = now / 1000;
    char buf[128];
    size_t n = serializeJson(t, buf);
    mqtt.publish(TOPIC_TELEM, buf, n);
  }

  delay(5);
}
