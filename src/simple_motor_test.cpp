#include <Arduino.h>

// モータピン
const int motorPin = 5;  // GPIO5をテスト用に使用

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== Simple Motor Test ===");
    
    // PWM設定 (50Hz, 12bit)
    ledcSetup(0, 50, 12);
    ledcAttachPin(motorPin, 0);
    
    Serial.println("Motor test starting...");
    Serial.println("GPIO5にESC信号線を接続してください");
    delay(3000);
    
    // ESC初期化 (停止信号)
    Serial.println("ESC停止信号送信中...");
    ledcWrite(0, 204);  // 1000μs (停止)
    delay(2000);
    
    Serial.println("テスト開始 - 3秒毎に信号を変更します");
}

void loop() {
    static int step = 0;
    static unsigned long lastChange = 0;
    
    if (millis() - lastChange > 3000) {
        switch(step) {
            case 0:
                Serial.println("停止信号 (1000μs)");
                ledcWrite(0, 204);  // 1000μs
                break;
            case 1:
                Serial.println("低速信号 (1200μs)");
                ledcWrite(0, 245);  // 1200μs
                break;
            case 2:
                Serial.println("中速信号 (1500μs)");
                ledcWrite(0, 307);  // 1500μs
                break;
            case 3:
                Serial.println("高速信号 (1800μs)");
                ledcWrite(0, 368);  // 1800μs
                break;
            case 4:
                Serial.println("最大信号 (2000μs)");
                ledcWrite(0, 409);  // 2000μs
                break;
        }
        
        step = (step + 1) % 5;
        lastChange = millis();
    }
}