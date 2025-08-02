#!/usr/bin/env python3
"""
UDP Test Sender for ESP32-S3 100% Thrust Test
Sends different EEG values to test motor control
"""

import socket
import time
import sys

def send_udp_test_values():
    # ESP32-S3のIPアドレス（WiFi接続後に表示される）
    # ルーターの接続済みデバイス一覧で確認するか、ESP32-S3のシリアル出力で確認
    esp32_ip = "172.21.128.229"  # ESP32-S3の実際のIPアドレス
    udp_port = 4210
    
    # UDPソケット作成
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    print("🚁 ESP32-S3 100% Thrust Test Sender")
    print(f"📡 Target: {esp32_ip}:{udp_port}")
    print("⚠️  注意: モーターが最大100%で回転する可能性があります！")
    print("")
    
    test_values = [
        (0.5, "強い左回転 (FL/RL=100%, FR/RR=5%)"),
        (1.5, "左回転 (FL/RL=80%, FR/RR=20%)"),
        (2.5, "直進浮上 (全モーター70%)"),  
        (3.5, "右回転 (FL/RL=20%, FR/RR=80%)"),
        (4.5, "強い右回転 (FL/RL=5%, FR/RR=100%)")
    ]
    
    try:
        while True:
            for value, description in test_values:
                message = str(value)
                
                print(f"📤 送信: {value} -> {description}")
                sock.sendto(message.encode(), (esp32_ip, udp_port))
                
                # 5秒間待機（モーターの音や振動を確認する時間）
                for i in range(5, 0, -1):
                    print(f"   ⏳ 次の値まで {i}秒...", end="\r")
                    time.sleep(1)
                print("")
                
    except KeyboardInterrupt:
        print("\n🛑 テスト停止")
    except Exception as e:
        print(f"❌ エラー: {e}")
        print("💡 ESP32-S3のIPアドレスを確認してください")
    finally:
        sock.close()

if __name__ == "__main__":
    if len(sys.argv) > 1:
        # コマンドライン引数でIPアドレス指定可能
        esp32_ip = sys.argv[1]
        print(f"🔄 IPアドレスを変更: {esp32_ip}")
    
    send_udp_test_values()