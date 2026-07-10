// StampFly Wi-Fi RC — Android/スマホからのリモコン操作ファーム (self-contained).
//
// 概要 ---------------------------------------------------------------------
// ESP32-S3 を Wi-Fi アクセスポイント化し、スマホのブラウザから操作する。
//   1. スマホを Wi-Fi「StampFly-RC」に接続 (パスワード: stampfly123)
//   2. ブラウザで http://192.168.4.1/ を開く (インストール不要・PWA可)
//   3. 仮想スティック / モーターduty スライダーで操作、センサー値をリアルタイム表示
//
// 通信は WebSocket(port 81)で双方向:
//   スマホ → ドローン : 制御コマンド JSON (20Hz)
//   ドローン → スマホ : センサーテレメトリ JSON (~15Hz)
//
// 純正 ATOM コントローラは ESP-NOW を使うが、スマホは ESP-NOW を話せないため
// Wi-Fi(soft-AP)+WebSocket に置き換えている。ブローカ不要・インターネット不要で
// 低遅延。既存の eeg_mqtt ファームと同じく単一環境でビルドする自己完結構成。
//
// モーターPWM は flight_control.cpp と完全一致 (150kHz / 8bit, pins 5/42/10/41,
// duty = 255*d)。IMU(BMI270) を実読みして加速度・角速度・姿勢角を表示する。
//
// Build/flash:  pio run -e wifi_rc -t upload
//
// 安全 (SAFETY): 初回テストは必ずプロペラを外すこと。モーターが回るのは
//   「ARM 済み」かつ「直近 CMD_TIMEOUT_MS 以内にコマンド受信」かつ
//   「連続稼働 RUN_LIMIT_MS 未満」のときのみ。各モーターは MAX_DUTY で上限クランプ。
//   通信断・タブを閉じる・STOP ボタンで即停止する。

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <math.h>
#include "imu.hpp"
#include "wifi_rc_page.h"

// ---- Wi-Fi soft-AP --------------------------------------------------------
static const char* AP_SSID = "StampFly-RC";
static const char* AP_PASS = "stampfly123";   // 8文字以上必須
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);

WebServer http(80);
WebSocketsServer ws(81);

// ---- Motor PWM (flight_control.cpp と一致) --------------------------------
static const int pwmFrontLeft = 5, pwmFrontRight = 42, pwmRearLeft = 10, pwmRearRight = 41;
static const int FL = 0, FR = 1, RL = 2, RR = 3;   // ledc channels
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
static float g_duty[4] = {0, 0, 0, 0};   // last written duty per motor (for telemetry)
static void set_duty(int ch, float duty) {
  if (duty < 0.0f) duty = 0.0f;
  if (duty > 1.0f) duty = 1.0f;
  g_duty[ch] = duty;
  ledcWrite(ch, (uint32_t)(255.0f * duty));
}
static void motors_off() {
  set_duty(FL, 0); set_duty(FR, 0); set_duty(RL, 0); set_duty(RR, 0);
}

// ---- Status LED -----------------------------------------------------------
#define LED_PIN 39            // StampFly onboard WS2812 (bottom). 21 on bare devkit.
#define NUM_LEDS 2
CRGB leds[NUM_LEDS];
static void led(uint32_t rgb) {
  CRGB c((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
  leds[0] = c; leds[1] = c;
  FastLED.show();
}

// ---- Safety limits --------------------------------------------------------
static const float MAX_DUTY = 0.35f;          // モーターduty ハード上限
static const float THROTTLE_SCALE = 0.60f;    // スロットル1.0 → duty 0.60 まで
static const float MIX_GAIN = 0.15f;          // roll/pitch 混合の最大寄与
static const float YAW_GAIN = 0.12f;          // yaw 混合の最大寄与
static const unsigned long CMD_TIMEOUT_MS = 600;    // 無通信で停止
static const unsigned long RUN_LIMIT_MS = 20000;    // ARM連続稼働の上限
static const unsigned long TELEM_INTERVAL_MS = 66;  // ~15Hz

// ---- Control state (WebSocket callback で更新) ----------------------------
struct Cmd {
  bool armed = false;
  bool motorMode = false;         // false=飛行mix, true=モーター個別
  float t = 0, r = 0, p = 0, y = 0;   // t:0..1, r/p/y:-1..1
  float m[4] = {0, 0, 0, 0};          // motor mode duty 0..1 (FL,FR,RL,RR)
};
static volatile bool g_have_cmd = false;
static Cmd g_cmd;
static unsigned long g_last_cmd = 0;
static unsigned long g_run_start = 0;
static bool g_running = false;

// ---- 姿勢推定 (相補フィルタ) ----------------------------------------------
static float g_roll = 0, g_pitch = 0;   // deg
static unsigned long g_att_t = 0;
static void update_attitude() {
  imu_update();
  float ax = imu_get_acc_x(), ay = imu_get_acc_y(), az = imu_get_acc_z();  // g
  float gx = imu_get_gyro_x(), gy = imu_get_gyro_y();                      // rad/s
  unsigned long now = micros();
  float dt = g_att_t ? (now - g_att_t) * 1e-6f : 0.0025f;
  g_att_t = now;
  if (dt > 0.1f) dt = 0.1f;
  float roll_acc = atan2f(ay, az) * 57.2958f;
  float pitch_acc = atan2f(-ax, sqrtf(ay * ay + az * az)) * 57.2958f;
  const float A = 0.98f;   // gyro寄与
  g_roll = A * (g_roll + gx * 57.2958f * dt) + (1 - A) * roll_acc;
  g_pitch = A * (g_pitch + gy * 57.2958f * dt) + (1 - A) * pitch_acc;
}

// ---- モーターミキサー ------------------------------------------------------
// X配置クアッド。t:スロットル r:ロール(右+) p:ピッチ(前+) y:ヨー(CW+)。
// 符号は機体に合わせて要検証(プロペラを外して確認すること)。
static void drive_mix(const Cmd& c) {
  float t = c.t * THROTTLE_SCALE;
  float r = c.r * MIX_GAIN, p = c.p * MIX_GAIN, y = c.y * YAW_GAIN;
  float fl = t - p + r - y;   // 前左 : CW
  float fr = t - p - r + y;   // 前右 : CCW
  float rl = t + p + r + y;   // 後左 : CCW
  float rr = t + p - r - y;   // 後右 : CW
  set_duty(FL, fminf(fl, MAX_DUTY));
  set_duty(FR, fminf(fr, MAX_DUTY));
  set_duty(RL, fminf(rl, MAX_DUTY));
  set_duty(RR, fminf(rr, MAX_DUTY));
}
static void drive_motor(const Cmd& c) {
  set_duty(FL, fminf(c.m[0], MAX_DUTY));
  set_duty(FR, fminf(c.m[1], MAX_DUTY));
  set_duty(RL, fminf(c.m[2], MAX_DUTY));
  set_duty(RR, fminf(c.m[3], MAX_DUTY));
}

// ---- WebSocket 受信 --------------------------------------------------------
static void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[ws] client %u connected\r\n", num);
      break;
    case WStype_DISCONNECTED:
      Serial.printf("[ws] client %u disconnected\r\n", num);
      // 接続クライアントが居なくなったら安全側へ
      if (ws.connectedClients() == 0) { g_cmd.armed = false; }
      break;
    case WStype_TEXT: {
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, payload, len)) return;   // 不正JSONは無視
      if (doc["stop"] | false) {                        // 緊急停止
        g_cmd.armed = false;
        g_cmd.t = g_cmd.r = g_cmd.p = g_cmd.y = 0;
        for (int i = 0; i < 4; i++) g_cmd.m[i] = 0;
        g_last_cmd = millis();
        g_have_cmd = true;
        return;
      }
      Cmd c;
      c.armed = doc["arm"] | false;
      const char* mode = doc["mode"] | "mix";
      c.motorMode = (strcmp(mode, "motor") == 0);
      c.t = doc["t"] | 0.0f;
      c.r = doc["r"] | 0.0f;
      c.p = doc["p"] | 0.0f;
      c.y = doc["y"] | 0.0f;
      JsonArrayConst ma = doc["m"];
      if (!ma.isNull()) for (int i = 0; i < 4 && i < (int)ma.size(); i++) c.m[i] = ma[i] | 0.0f;
      // range guard
      c.t = constrain(c.t, 0.0f, 1.0f);
      c.r = constrain(c.r, -1.0f, 1.0f);
      c.p = constrain(c.p, -1.0f, 1.0f);
      c.y = constrain(c.y, -1.0f, 1.0f);
      for (int i = 0; i < 4; i++) c.m[i] = constrain(c.m[i], 0.0f, 1.0f);
      g_cmd = c;
      g_last_cmd = millis();
      g_have_cmd = true;
      break;
    }
    default: break;
  }
}

static void handleRoot() { http.send_P(200, "text/html", WIFI_RC_PAGE); }

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("\r\n=== StampFly Wi-Fi RC ===");

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(40);
  led(0x111111);

  init_pwm();
  motors_off();

  // IMU (失敗しても操作機能は継続できるよう try 相当のガードは無いが、
  // imu_init 内で失敗時は while(1) するため配線前提)。
  imu_init();
  Serial.println("IMU init done");

  // soft-AP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP: %s  pass: %s\r\n", AP_SSID, AP_PASS);
  Serial.printf("URL: http://%s/\r\n", WiFi.softAPIP().toString().c_str());

  http.on("/", handleRoot);
  http.onNotFound(handleRoot);   // どのパスでも操作ページを返す
  http.begin();

  ws.begin();
  ws.onEvent(onWsEvent);

  led(0x001133);   // 待機(青)
  Serial.println("Ready. Connect Wi-Fi and open the URL. Props OFF for first test!");
}

void loop() {
  http.handleClient();
  ws.loop();

  const unsigned long now = millis();
  const bool fresh = g_have_cmd && (now - g_last_cmd) < CMD_TIMEOUT_MS;

  if (g_cmd.armed && fresh) {
    if (!g_running) { g_running = true; g_run_start = now; }
    if (now - g_run_start > RUN_LIMIT_MS) {
      motors_off();          // 連続稼働上限 → 再ARMするまで停止
      led(0xFF6600);
    } else {
      if (g_cmd.motorMode) drive_motor(g_cmd);
      else drive_mix(g_cmd);
      led(0x330000);         // 稼働中(赤)
    }
  } else {
    if (g_running) Serial.println("disarm/no-data -> motors off");
    g_running = false;
    motors_off();
    led(g_cmd.armed ? 0x331100 : 0x001133);   // armだがデータ古い=橙 / 待機=青
  }

  // テレメトリ送信 (~15Hz)
  static unsigned long lastTelem = 0;
  if (now - lastTelem >= TELEM_INTERVAL_MS) {
    lastTelem = now;
    update_attitude();
    if (ws.connectedClients() > 0) {
      StaticJsonDocument<256> t;
      t["ax"] = roundf(imu_get_acc_x() * 100) / 100;
      t["ay"] = roundf(imu_get_acc_y() * 100) / 100;
      t["az"] = roundf(imu_get_acc_z() * 100) / 100;
      t["gx"] = roundf(imu_get_gyro_x() * 57.2958f * 10) / 10;   // deg/s
      t["gy"] = roundf(imu_get_gyro_y() * 57.2958f * 10) / 10;
      t["gz"] = roundf(imu_get_gyro_z() * 57.2958f * 10) / 10;
      t["roll"] = roundf(g_roll * 10) / 10;
      t["pitch"] = roundf(g_pitch * 10) / 10;
      t["armed"] = g_cmd.armed && g_running;
      JsonArray du = t.createNestedArray("duty");
      for (int i = 0; i < 4; i++) du.add(roundf(g_duty[i] * 1000) / 1000);
      char buf[256];
      size_t n = serializeJson(t, buf);
      ws.broadcastTXT(buf, n);
    }
  }
}
