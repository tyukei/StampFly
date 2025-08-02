#include <Arduino.h>

// モータピン
const int motorPins[] = {5, 42, 10, 41};
const char* motorNames[] = {"FL", "FR", "RL", "RR"};

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== DEBUG MOTOR CONTROL ===");
    Serial.println("デバッグ目的: EEG値に関係なく回転する原因を特定");
    
    // 全モーターを完全停止に初期化
    for (int i = 0; i < 4; i++) {
        ledcSetup(i, 50, 12);
        ledcAttachPin(motorPins[i], i);
        ledcWrite(i, 204);  // 1000μs = 完全停止
        Serial.printf("Motor %s (Pin %d, Channel %d): PWM=204 (STOP)\n", 
                     motorNames[i], motorPins[i], i);
    }
    
    Serial.println("\n全モーター停止完了");
    Serial.println("5秒後に手動で段階的テスト開始...");
    delay(5000);
}

void loop() {
    static int testStep = 0;
    static unsigned long lastChange = 0;
    
    if (millis() - lastChange > 5000) {  // 5秒間隔
        Serial.printf("\n=== TEST STEP %d ===\n", testStep);
        
        switch(testStep) {
            case 0:
                Serial.println("全モーター完全停止 (PWM=204, 1000μs)");
                for (int i = 0; i < 4; i++) {
                    ledcWrite(i, 204);
                    Serial.printf("%s: PWM=204 (STOP)\n", motorNames[i]);
                }
                break;
                
            case 1:
                Serial.println("全モーター微動 (PWM=225, 1100μs)");
                for (int i = 0; i < 4; i++) {
                    ledcWrite(i, 225);
                    Serial.printf("%s: PWM=225 (VERY SLOW)\n", motorNames[i]);
                }
                break;
                
            case 2:
                Serial.println("全モーター低速 (PWM=245, 1200μs)");
                for (int i = 0; i < 4; i++) {
                    ledcWrite(i, 245);
                    Serial.printf("%s: PWM=245 (SLOW)\n", motorNames[i]);
                }
                break;
                
            case 3:
                Serial.println("全モーター停止に戻る (PWM=204, 1000μs)");
                for (int i = 0; i < 4; i++) {
                    ledcWrite(i, 204);
                    Serial.printf("%s: PWM=204 (STOP)\n", motorNames[i]);
                }
                break;
                
            default:
                Serial.println("テスト完了 - 停止状態維持");
                testStep = 3;  // 停止状態をループ
                break;
        }
        
        Serial.println("現在のモーターの物理的状態を確認してください");
        Serial.println("----------------------------------------------");
        
        testStep++;
        lastChange = millis();
    }
}