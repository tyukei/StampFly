#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi設定
const char* ssid = "OPPORenon";
const char* password = "bxyk6037";

// UDP設定
WiFiUDP udp;
const int UDP_PORT = 4210;
char packetBuffer[255];

void setup() {
  // 元のコードと同じくSerial.begin使用
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("======================");
  Serial.println("ESP32-S3 UDP Test");
  Serial.println("======================");
  Serial.print("Chip Model: ");
  Serial.println(ESP.getChipModel());
  
  Serial.println("Starting WiFi connection...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts % 10 == 0) {
      Serial.print(" Attempt ");
      Serial.println(attempts);
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // UDP開始
    if (udp.begin(UDP_PORT)) {
      Serial.print("UDP server started on port: ");
      Serial.println(UDP_PORT);
    } else {
      Serial.println("Failed to start UDP server!");
    }
  } else {
    Serial.println();
    Serial.println("WiFi connection failed!");
  }
  
  Serial.println("Setup complete! Waiting for UDP packets...");
}

void loop() {
  static unsigned long lastPrint = 0;
  static unsigned long packetCount = 0;
  
  // 10秒毎に生存確認
  if (millis() - lastPrint > 10000) {
    Serial.print("Running... Time: ");
    Serial.print(millis() / 1000);
    Serial.print("s, Packets: ");
    Serial.println(packetCount);
    lastPrint = millis();
  }
  
  // UDPパケット受信チェック
  int packetSize = udp.parsePacket();
  if (packetSize) {
    packetCount++;
    Serial.println("--- PACKET RECEIVED ---");
    
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    
    Serial.print("From: ");
    Serial.print(udp.remoteIP().toString());
    Serial.print(":");
    Serial.println(udp.remotePort());
    Serial.print("Data: ");
    Serial.println(packetBuffer);
    
    float eegValue = atof(packetBuffer);
    Serial.print("EEG Value: ");
    Serial.println(eegValue, 2);
    Serial.println("----------------------");
  }
  
  delay(10);
}