#!/usr/bin/env python3
import os
from pathlib import Path

# .envファイルのパス
env_file = Path(__file__).parent.parent / '.env'

if env_file.exists():
    with open(env_file, 'r') as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#') and '=' in line:
                key, value = line.split('=', 1)
                # WiFi設定をビルドフラグとして出力
                if key == 'WIFI_SSID':
                    print(f'-DWIFI_SSID=\\"{value}\\"')
                elif key == 'WIFI_PASSWORD':
                    print(f'-DWIFI_PASSWORD=\\"{value}\\"')
else:
    print("# .env file not found")