#!/usr/bin/env python3
import socket
import time
import random
# M5Stamp S3のIPアドレス（起動時にシリアルモニターで確認）
TARGET_IP = "172.21.128.229"  # 実際のIPに変更
TARGET_PORT = 4210
# UDPソケットを作成
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
print(f"Sending EEG data to {TARGET_IP}:{TARGET_PORT}")
try:
    while True:
        values = [0.3, 1.0, 2.0, 3.0, 5.0]
        for value in values:
            # EEG値をシミュレート (0.0-5.0)
            eeg_value = value + random.uniform(-0.1, 0.1)
            # 文字列として送信
            message = str(eeg_value)
            sock.sendto(message.encode(), (TARGET_IP, TARGET_PORT))
            print(f"Sent EEG: {eeg_value:.2f}")
            # 1秒待機
            time.sleep(1)
except KeyboardInterrupt:
    print("\nStopping...")
finally:
    sock.close()

