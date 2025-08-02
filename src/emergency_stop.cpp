#include <Arduino.h>

// モータピン
const int motorPins[] = {5, 42, 10, 41};

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== EMERGENCY STOP - ALL MOTORS OFF ===");
    
    // 全モーターピンを停止信号に設定
    for (int i = 0; i < 4; i++) {
        ledcSetup(i, 50, 12);
        ledcAttachPin(motorPins[i], i);
        ledcWrite(i, 204);  // 1000μs = 停止信号
    }
    
    Serial.println("All motors stopped!");
    Serial.println("Motors will remain stopped.");
}

void loop() {
    // 何もしない - モーター停止状態を維持
    delay(1000);
    Serial.println("Motors stopped - safe mode");
}