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
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("======================");
  Serial.println("ESP32-S3 Debug Test");
  Serial.println("======================");
  Serial.print("Chip Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Chip Revision: ");
  Serial.println(ESP.getChipRevision());
  Serial.print("Flash Size: ");
  Serial.println(ESP.getFlashChipSize());
  
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
    Serial.print("Subnet Mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
    
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
    Serial.print("Status: ");
    Serial.println(WiFi.status());
  }
  
  Serial.println("Setup complete!");
  Serial.println("======================");
}

void loop() {
  static unsigned long lastPrint = 0;
  
  // 5秒毎に生存確認メッセージ
  if (millis() - lastPrint > 5000) {
    Serial.print("Running... Time: ");
    Serial.print(millis() / 1000);
    Serial.println("s");
    lastPrint = millis();
  }
  
  // UDPパケット受信チェック
  int packetSize = udp.parsePacket();
  if (packetSize) {
    Serial.println("------------------------");
    Serial.print("Packet received! Size: ");
    Serial.println(packetSize);
    
    // パケット読み取り
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    
    // 送信者情報
    Serial.print("From: ");
    Serial.print(udp.remoteIP().toString());
    Serial.print(":");
    Serial.println(udp.remotePort());
    Serial.print("Raw data: [");
    Serial.print(packetBuffer);
    Serial.println("]");
    
    // EEG値として解析
    float eegValue = atof(packetBuffer);
    Serial.print("Parsed as EEG value: ");
    Serial.println(eegValue, 2);
    Serial.println("------------------------");
  }
  
  delay(10);
}