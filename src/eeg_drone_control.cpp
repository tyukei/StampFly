#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "flight_control.hpp"
#include "led.hpp"

// WiFi設定（.envから読み込み）
#ifndef WIFI_SSID
#define WIFI_SSID "default_ssid"
#endif
#ifndef WIFI_PASSWORD  
#define WIFI_PASSWORD "default_password"
#endif

static const char* eeg_ssid = WIFI_SSID;
static const char* eeg_password = WIFI_PASSWORD;

// UDP設定
WiFiUDP udp;
const int UDP_PORT = 4210;
char packetBuffer[255];

// EEG制御変数
static float eeg_current_value = 0.0f;
static unsigned long eeg_last_update = 0;
static bool eeg_mode_active = false;

// ドローン制御変数
extern volatile float Yaw_rate_reference;
extern volatile float Thrust_command;
extern volatile float Roll_rate_reference; 
extern volatile float Pitch_rate_reference;
extern uint8_t Mode;

// EEG値に基づく回転速度マッピング
float getYawRateFromEEG(float eegValue) {
  // EEG値を回転速度に変換（ラジアン/秒）
  if (eegValue < 0.5f) {
    return 0.0f;           // 停止
  }
  else if (eegValue < 1.5f) {
    return 0.5f;          // ゆっくり右回転
  }
  else if (eegValue < 2.5f) {
    return 1.0f;          // 中速右回転
  }
  else if (eegValue < 4.0f) {
    return 2.0f;          // 高速右回転
  }
  else {
    return -1.0f;         // 左回転（最高集中時）
  }
}

// EEG値に基づくスロットル制御
float getThrustFromEEG(float eegValue) {
  // 基本浮上力 + EEG値による追加推力
  float base_thrust = 0.55f; // 基本浮上推力
  float eeg_thrust = (eegValue / 5.0f) * 0.2f; // EEG値による追加推力
  return base_thrust + eeg_thrust;
}

// EEG値に基づくLED制御
void updateEEGLED(float eegValue) {
    if (eegValue < 0.5f) {
        esp_led(0x000044, 1);  // 青色（低集中）
    }
    else if (eegValue < 1.5f) {
        esp_led(0x004400, 1);  // 緑色（中集中）
    }
    else if (eegValue < 2.5f) {
        esp_led(0x440000, 1);  // 赤色（高集中）
    }
    else if (eegValue < 4.0f) {
        esp_led(0x440044, 1);  // マゼンタ（超高集中）
    }
    else {
        esp_led(0x444444, 1);  // 白色（最高集中）
    }
    led_show();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("==============================");
    Serial.println("EEG Drone Control System");
    Serial.println("==============================");
    
    // WiFi接続
    Serial.printf("Connecting to WiFi: %s\r\n", eeg_ssid);
    WiFi.begin(eeg_ssid, eeg_password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("WiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println();
        Serial.println("WiFi connection failed!");
        return;
    }
    
    // UDP開始
    if (udp.begin(UDP_PORT)) {
        Serial.printf("UDP server started on port: %d\r\n", UDP_PORT);
    }
    
    // ドローンシステム初期化
    init_copter();
    
    Serial.println("EEG Drone Control Ready!");
    Serial.println("EEG Control Mapping:");
    Serial.println("0.0-0.5: Stop (Blue LED)");
    Serial.println("0.5-1.5: Slow Right Turn (Green LED)");
    Serial.println("1.5-2.5: Medium Right Turn (Red LED)");
    Serial.println("2.5-4.0: Fast Right Turn (Magenta LED)");
    Serial.println("4.0+: Left Turn (White LED)");
    Serial.println("------------------------------");
}

void loop() {
    static unsigned long lastPrint = 0;
    static unsigned long packetCount = 0;
    
    // 10秒毎に状態表示
    if (millis() - lastPrint > 10000) {
        Serial.printf("Running... Packets received: %lu, Current EEG: %.2f\r\n", 
                     packetCount, eeg_current_value);
        lastPrint = millis();
    }
    
    // UDPパケット受信チェック
    int packetSize = udp.parsePacket();
    if (packetSize) {
        packetCount++;
        
        int len = udp.read(packetBuffer, 255);
        if (len > 0) {
            packetBuffer[len] = 0;
        }
        
        float newEegValue = atof(packetBuffer);
        
        if (newEegValue >= 0.0f && newEegValue <= 10.0f) { // 妥当性チェック
            eeg_current_value = newEegValue;
            eeg_last_update = millis();
            eeg_mode_active = true;
            
            // EEG値に基づく制御指令生成
            float yaw_rate = getYawRateFromEEG(eeg_current_value);
            float thrust = getThrustFromEEG(eeg_current_value);
            
            // ドローン制御値を更新（安全のため条件付き）
            if (Mode == FLIGHT_MODE || Mode == PARKING_MODE) {
                Yaw_rate_reference = yaw_rate;
                
                // 安全な推力範囲でのみ制御
                if (thrust >= 0.5f && thrust <= 0.8f) {
                    Thrust_command = thrust * 3.7f; // BATTERY_VOLTAGE換算
                }
                
                // 水平維持（Roll/Pitchは0に保持）
                Roll_rate_reference = 0.0f;
                Pitch_rate_reference = 0.0f;
            }
            
            // LED更新
            updateEEGLED(eeg_current_value);
            
            Serial.printf("EEG: %.2f -> Yaw Rate: %.2f rad/s, Thrust: %.2f\r\n", 
                         eeg_current_value, yaw_rate, thrust);
        }
    }
    
    // EEGデータタイムアウトチェック（5秒間データなしで停止）
    if (eeg_mode_active && (millis() - eeg_last_update > 5000)) {
        Serial.println("EEG data timeout - stopping rotation");
        Yaw_rate_reference = 0.0f;
        eeg_mode_active = false;
        esp_led(0x441100, 1); // オレンジ色でタイムアウト表示
        led_show();
    }
    
    // ドローンメインループ実行
    loop_400Hz();
    
    delay(1);
}