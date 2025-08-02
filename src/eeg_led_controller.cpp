#include <WiFi.h>
#include <WiFiUdp.h>
#include <FastLED.h>

// WiFi設定
const char* ssid = "OPPORenon";
const char* password = "bxyk6037";

// UDP設定
WiFiUDP udp;
const int UDP_PORT = 4210;
char packetBuffer[255];

// LED設定
#define LED_PIN 48      // ESP32-S3のRGB LEDピン
#define NUM_LEDS 1      // 1個のLED
CRGB leds[NUM_LEDS];

// EEG色定義
CRGB getEEGColor(float eegValue) {
  if (eegValue < 0.5f) return CRGB::Blue;        // 低集中
  else if (eegValue < 1.5f) return CRGB::Green;   // 中集中
  else if (eegValue < 2.5f) return CRGB::Red;     // 高集中
  else if (eegValue < 4.0f) return CRGB::Magenta; // 超高集中
  else return CRGB::White;                         // 最高集中
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // LED初期化
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);  // 明るさ調整
  leds[0] = CRGB::Yellow;     // 起動時は黄色
  FastLED.show();
  
  Serial.println("======================");
  Serial.println("EEG-LED Controller");
  Serial.println("======================");
  
  // WiFi接続
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // UDP開始
  if (udp.begin(UDP_PORT)) {
    Serial.print("UDP server started on port: ");
    Serial.println(UDP_PORT);
  }
  
  Serial.println("Ready for EEG data...");
  
  // 準備完了 - 緑色で2秒点灯
  leds[0] = CRGB::Green;
  FastLED.show();
  delay(2000);
}

void loop() {
  static unsigned long lastPrint = 0;
  static unsigned long packetCount = 0;
  
  // 10秒毎に状態表示
  if (millis() - lastPrint > 10000) {
    Serial.print("Running... Packets: ");
    Serial.println(packetCount);
    lastPrint = millis();
  }
  
  // UDPパケット受信チェック
  int packetSize = udp.parsePacket();
  if (packetSize) {
    packetCount++;
    
    // パケット読み取り
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    
    // EEG値として解析
    float eegValue = atof(packetBuffer);
    
    // LED色を更新
    CRGB newColor = getEEGColor(eegValue);
    leds[0] = newColor;
    FastLED.show();
    
    // ログ出力
    Serial.print("EEG: ");
    Serial.print(eegValue, 2);
    Serial.print(" -> LED: ");
    if (eegValue < 0.5f) Serial.println("Blue (Low)");
    else if (eegValue < 1.5f) Serial.println("Green (Mid)");
    else if (eegValue < 2.5f) Serial.println("Red (High)");
    else if (eegValue < 4.0f) Serial.println("Magenta (Very High)");
    else Serial.println("White (Max)");
  }
  
  delay(10);
}