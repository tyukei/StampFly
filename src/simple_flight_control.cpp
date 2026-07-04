#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi設定
#ifndef WIFI_SSID
#define WIFI_SSID "default_ssid"
#endif
#ifndef WIFI_PASSWORD  
#define WIFI_PASSWORD "default_password"
#endif

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// UDP設定
WiFiUDP udp;
const int UDP_PORT = 4210;
char packetBuffer[255];

// モータピン
const int motorPins[] = {5, 42, 10, 41};
const char* motorNames[] = {"FL", "FR", "RL", "RR"};

// LED
const int LED_PINS[] = {2, 8, 48};

// 制御変数
float current_eeg_value = 0.0f;
unsigned long last_eeg_update = 0;
bool eeg_active = false;

void setMotorESC(int channel, float throttle) {
    // 最小5%、最大100%
    if (throttle < 0.05f) throttle = 0.05f;
    if (throttle > 1.0f) throttle = 1.0f;
    
    uint32_t pulse_min = (uint32_t)(4096 * 0.05);
    uint32_t pulse_max = (uint32_t)(4096 * 0.10);
    uint32_t pulse_width = pulse_min + (uint32_t)((pulse_max - pulse_min) * throttle);
    
    ledcWrite(channel, pulse_width);
}

void setLED(uint8_t r, uint8_t g, uint8_t b) {
    digitalWrite(LED_PINS[0], r > 127 ? HIGH : LOW);
    digitalWrite(LED_PINS[1], g > 127 ? HIGH : LOW);
    digitalWrite(LED_PINS[2], b > 127 ? HIGH : LOW);
}

void controlMotorsForFlight(float eegValue) {
    // 浮力重視設定（ほぼ100%推力）
    float thrust[4];  // FL, FR, RL, RR
    
    if (eegValue < 1.0f) {
        // 左回転（浮力維持しながら）
        thrust[0] = 1.00f; // FL 100%
        thrust[1] = 0.85f; // FR 85%
        thrust[2] = 1.00f; // RL 100%
        thrust[3] = 0.85f; // RR 85%
        setLED(0, 0, 255); // 青
        Serial.println(">>> 左回転飛行: FL/RL=100%, FR/RR=85%");
    }
    else if (eegValue < 2.0f) {
        // 緩やかな左回転
        thrust[0] = 0.95f; // FL 95%
        thrust[1] = 0.90f; // FR 90%
        thrust[2] = 0.95f; // RL 95%
        thrust[3] = 0.90f; // RR 90%
        setLED(0, 255, 255); // シアン
        Serial.println(">>> 緩やかな左回転: FL/RL=95%, FR/RR=90%");
    }
    else if (eegValue < 3.0f) {
        // 上昇飛行（最大推力）
        thrust[0] = 1.00f; // FL 100%
        thrust[1] = 1.00f; // FR 100%
        thrust[2] = 1.00f; // RL 100%
        thrust[3] = 1.00f; // RR 100%
        setLED(0, 255, 0); // 緑
        Serial.println(">>> 最大上昇: 全モーター100%");
    }
    else if (eegValue < 4.0f) {
        // 緩やかな右回転
        thrust[0] = 0.90f; // FL 90%
        thrust[1] = 0.95f; // FR 95%
        thrust[2] = 0.90f; // RL 90%
        thrust[3] = 0.95f; // RR 95%
        setLED(255, 255, 0); // 黄
        Serial.println(">>> 緩やかな右回転: FL/RL=90%, FR/RR=95%");
    }
    else {
        // 右回転（浮力維持しながら）
        thrust[0] = 0.85f; // FL 85%
        thrust[1] = 1.00f; // FR 100%
        thrust[2] = 0.85f; // RL 85%
        thrust[3] = 1.00f; // RR 100%
        setLED(255, 0, 0); // 赤
        Serial.println(">>> 右回転飛行: FL/RL=85%, FR/RR=100%");
    }
    
    // モーター制御実行
    for (int i = 0; i < 4; i++) {
        setMotorESC(i, thrust[i]);
    }
    
    Serial.printf("EEG: %.2f | 推力 - FL:%.0f%% FR:%.0f%% RL:%.0f%% RR:%.0f%%\n", 
                 eegValue, thrust[0]*100, thrust[1]*100, thrust[2]*100, thrust[3]*100);
    Serial.println("🚁 飛行モード: 浮力重視設定");
    Serial.println("--------------------------------------------------");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=========================================");
    Serial.println("🚁 SIMPLE FLIGHT CONTROL");
    Serial.println("浮力重視・簡単飛行制御システム");
    Serial.println("=========================================");
    
    // LED初期化
    for (int i = 0; i < 3; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }
    
    // PWM初期化
    for (int i = 0; i < 4; i++) {
        ledcSetup(i, 50, 12);
        ledcAttachPin(motorPins[i], i);
        setMotorESC(i, 0.05f); // 初期は最小
    }
    
    // WiFi接続
    Serial.printf("WiFi接続中: %s\n", ssid);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi接続成功!");
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        
        if (udp.begin(UDP_PORT)) {
            Serial.printf("UDP server started on port: %d\n", UDP_PORT);
        }
    }
    
    Serial.println("\n=== 🚁 飛行制御システム準備完了 ===");
    Serial.println("EEG値 0-1: 左回転飛行（FL/RL=100%, FR/RR=85%）");
    Serial.println("EEG値 1-2: 緩やか左回転（FL/RL=95%, FR/RR=90%）");  
    Serial.println("EEG値 2-3: 最大上昇（全モーター100%）");
    Serial.println("EEG値 3-4: 緩やか右回転（FL/RL=90%, FR/RR=95%）");
    Serial.println("EEG値 4+:  右回転飛行（FL/RL=85%, FR/RR=100%）");
    Serial.println("====================================");
    
    // 警告表示
    Serial.println("\n*** ⚠️  飛行準備: 最大100%推力使用 ***");
    Serial.println("ドローンを安全な場所で飛行させてください！");
    
    // 起動表示
    for (int i = 0; i < 3; i++) {
        setLED(255, 255, 255);
        delay(300);
        setLED(0, 0, 0);  
        delay(300);
    }
}

void loop() {
    static unsigned long lastPrint = 0;
    static unsigned long packetCount = 0;
    
    // 3秒毎に状態表示
    if (millis() - lastPrint > 3000) {
        Serial.printf("\nStatus - Packets: %lu, EEG: %.2f, Active: %s\n", 
                     packetCount, current_eeg_value, eeg_active ? "FLYING" : "STANDBY");
        
        if (!eeg_active) {
            Serial.println("EEGデータ待機中 - 全モーター最小推力");
            for (int i = 0; i < 4; i++) {
                setMotorESC(i, 0.05f);
            }
            setLED(50, 50, 50); // 暗いグレー
        }
        
        lastPrint = millis();
    }
    
    // UDPパケット受信
    int packetSize = udp.parsePacket();
    if (packetSize) {
        packetCount++;
        
        int len = udp.read(packetBuffer, 255);
        if (len > 0) {
            packetBuffer[len] = 0;
        }
        
        float newEegValue = atof(packetBuffer);
        
        if (newEegValue >= 0.0f && newEegValue <= 10.0f) {
            current_eeg_value = newEegValue;
            last_eeg_update = millis();
            eeg_active = true;
            
            controlMotorsForFlight(current_eeg_value);
        }
    }
    
    // EEGタイムアウト（3秒）
    if (eeg_active && (millis() - last_eeg_update > 3000)) {
        Serial.println("\nEEGタイムアウト - 安全着陸モード");
        eeg_active = false;
        for (int i = 0; i < 4; i++) {
            setMotorESC(i, 0.05f);
        }
        setLED(255, 165, 0); // オレンジ
    }
    
    delay(10);
}