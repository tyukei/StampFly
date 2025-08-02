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
    if (throttle < 0.05f) throttle = 0.05f;
    if (throttle > 1.0f) throttle = 1.0f;  // 最大100%まで許可
    
    uint32_t pulse_min = (uint32_t)(4096 * 0.05);
    uint32_t pulse_max = (uint32_t)(4096 * 0.10);
    uint32_t pulse_width = pulse_min + (uint32_t)((pulse_max - pulse_min) * throttle);
    
    ledcWrite(channel, pulse_width);
}

void setLED(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PINS[i], (r > 127) ? HIGH : LOW);
    }
}

void controlMotorsWithEEG(float eegValue) {
    // 極端な推力差で明確な制御を実現
    float thrust[4];  // FL, FR, RL, RR
    
    if (eegValue < 1.0f) {
        // 強い左回転: FL/RL=最大, FR/RR=最小
        thrust[0] = 1.00f; // FL 100%!
        thrust[1] = 0.05f; // FR 5%  
        thrust[2] = 1.00f; // RL 100%!
        thrust[3] = 0.05f; // RR 5%
        setLED(255, 0, 255); // マゼンタ
        Serial.println(">>> 強い左回転: FL/RL=100%, FR/RR=5% (20倍差!)");
    }
    else if (eegValue < 2.0f) {
        // 左回転: FL/RL=高, FR/RR=低
        thrust[0] = 0.80f; // FL 80%
        thrust[1] = 0.20f; // FR 20%
        thrust[2] = 0.80f; // RL 80%
        thrust[3] = 0.20f; // RR 20%
        setLED(0, 0, 255); // 青
        Serial.println(">>> 左回転: FL/RL=80%, FR/RR=20% (4倍差)");
    }
    else if (eegValue < 3.0f) {
        // 直進: 全て高推力で浮上
        thrust[0] = 0.70f; // FL 70%
        thrust[1] = 0.70f; // FR 70%
        thrust[2] = 0.70f; // RL 70%
        thrust[3] = 0.70f; // RR 70%
        setLED(0, 255, 0); // 緑
        Serial.println(">>> 直進浮上: 全モーター70%");
    }
    else if (eegValue < 4.0f) {
        // 右回転: FL/RL=低, FR/RR=高
        thrust[0] = 0.20f; // FL 20%
        thrust[1] = 0.80f; // FR 80%
        thrust[2] = 0.20f; // RL 20%
        thrust[3] = 0.80f; // RR 80%
        setLED(255, 255, 0); // 黄
        Serial.println(">>> 右回転: FL/RL=20%, FR/RR=80% (4倍差)");
    }
    else {
        // 強い右回転: FL/RL=最小, FR/RR=最大
        thrust[0] = 0.05f; // FL 5%
        thrust[1] = 1.00f; // FR 100%!
        thrust[2] = 0.05f; // RL 5%
        thrust[3] = 1.00f; // RR 100%!
        setLED(255, 0, 0); // 赤
        Serial.println(">>> 強い右回転: FL/RL=5%, FR/RR=100% (20倍差!)");
    }
    
    // モーター制御実行
    for (int i = 0; i < 4; i++) {
        setMotorESC(i, thrust[i]);
    }
    
    Serial.printf("EEG: %.2f | 推力 - FL:%.0f%% FR:%.0f%% RL:%.0f%% RR:%.0f%%\n", 
                 eegValue, thrust[0]*100, thrust[1]*100, thrust[2]*100, thrust[3]*100);
    Serial.println("各モーターの音や振動の違いを確認してください");
    Serial.println("--------------------------------------------------");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=========================================");
    Serial.println("EXTREME MOTOR DIFFERENCE TEST");
    Serial.println("極端な推力差テスト（最大20倍差・100%推力）");
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
    
    Serial.println("\n=== 極端推力差テスト準備完了 ===");
    Serial.println("EEG値 0-1: FL/RL=100%, FR/RR=5% (20倍差!)");
    Serial.println("EEG値 1-2: FL/RL=80%, FR/RR=20% (4倍差)");  
    Serial.println("EEG値 2-3: 全モーター70% (直進浮上)");
    Serial.println("EEG値 3-4: FL/RL=20%, FR/RR=80% (4倍差)");
    Serial.println("EEG値 4+:  FL/RL=5%, FR/RR=100% (20倍差!)");
    Serial.println("=====================================");
    
    // 警告表示
    Serial.println("\n*** 警告: 最大推力100%使用 ***");
    Serial.println("プロペラが外れていることを確認してください！");
    
    // 起動表示
    for (int i = 0; i < 5; i++) {
        setLED(255, 0, 0);
        delay(200);
        setLED(0, 0, 0);  
        delay(200);
    }
}

void loop() {
    static unsigned long lastPrint = 0;
    static unsigned long packetCount = 0;
    
    // 3秒毎に状態表示
    if (millis() - lastPrint > 3000) {
        Serial.printf("\nStatus - Packets: %lu, EEG: %.2f, Active: %s\n", 
                     packetCount, current_eeg_value, eeg_active ? "YES" : "NO");
        
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
            
            controlMotorsWithEEG(current_eeg_value);
        }
    }
    
    // EEGタイムアウト（5秒）
    if (eeg_active && (millis() - last_eeg_update > 5000)) {
        Serial.println("\nEEGタイムアウト - 全モーター最小推力に戻る");
        eeg_active = false;
        for (int i = 0; i < 4; i++) {
            setMotorESC(i, 0.05f);
        }
        setLED(255, 165, 0); // オレンジ
    }
    
    delay(10);
}