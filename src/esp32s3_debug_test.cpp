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
  // ESP32-S3用のUSBシリアル初期化
  USBSerial.begin(115200);
  delay(3000);  // ESP32-S3は起動に時間がかかる場合がある
  
  USBSerial.println("======================");
  USBSerial.println("ESP32-S3 Debug Test");
  USBSerial.println("======================");
  USBSerial.print("Chip Model: ");
  USBSerial.println(ESP.getChipModel());
  USBSerial.print("Chip Revision: ");
  USBSerial.println(ESP.getChipRevision());
  USBSerial.print("Flash Size: ");
  USBSerial.println(ESP.getFlashChipSize());
  USBSerial.print("Free Heap: ");
  USBSerial.println(ESP.getFreeHeap());
  
  USBSerial.println("Starting WiFi connection...");
  USBSerial.print("SSID: ");
  USBSerial.println(ssid);
  
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    USBSerial.print(".");
    attempts++;
    if (attempts % 10 == 0) {
      USBSerial.print(" Attempt ");
      USBSerial.println(attempts);
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    USBSerial.println();
    USBSerial.println("WiFi Connected!");
    USBSerial.print("IP Address: ");
    USBSerial.println(WiFi.localIP());
    USBSerial.print("Subnet Mask: ");
    USBSerial.println(WiFi.subnetMask());
    USBSerial.print("Gateway: ");
    USBSerial.println(WiFi.gatewayIP());
    USBSerial.print("MAC Address: ");
    USBSerial.println(WiFi.macAddress());
    
    // UDP開始
    if (udp.begin(UDP_PORT)) {
      USBSerial.print("UDP server started on port: ");
      USBSerial.println(UDP_PORT);
    } else {
      USBSerial.println("Failed to start UDP server!");
    }
  } else {
    USBSerial.println();
    USBSerial.println("WiFi connection failed!");
    USBSerial.print("Status: ");
    USBSerial.println(WiFi.status());
  }
  
  USBSerial.println("Setup complete!");
  USBSerial.println("======================");
  USBSerial.println("Waiting for UDP packets...");
}

void loop() {
  static unsigned long lastPrint = 0;
  static unsigned long packetCount = 0;
  
  // 10秒毎に生存確認メッセージ
  if (millis() - lastPrint > 10000) {
    USBSerial.print("Running... Time: ");
    USBSerial.print(millis() / 1000);
    USBSerial.print("s, Free Heap: ");
    USBSerial.print(ESP.getFreeHeap());
    USBSerial.print(", Packets received: ");
    USBSerial.println(packetCount);
    lastPrint = millis();
  }
  
  // UDPパケット受信チェック
  int packetSize = udp.parsePacket();
  if (packetSize) {
    packetCount++;
    USBSerial.println("************************");
    USBSerial.print("Packet #");
    USBSerial.print(packetCount);
    USBSerial.print(" received! Size: ");
    USBSerial.println(packetSize);
    
    // パケット読み取り
    int len = udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    
    // 送信者情報
    USBSerial.print("From: ");
    USBSerial.print(udp.remoteIP().toString());
    USBSerial.print(":");
    USBSerial.println(udp.remotePort());
    USBSerial.print("Raw data: [");
    USBSerial.print(packetBuffer);
    USBSerial.println("]");
    
    // EEG値として解析
    float eegValue = atof(packetBuffer);
    USBSerial.print("Parsed as EEG value: ");
    USBSerial.println(eegValue, 2);
    USBSerial.println("************************");
  }
  
  delay(10);
}