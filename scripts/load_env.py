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
    # No .env (e.g. CI): emit nothing so no build flag is injected.
    # The firmware falls back to its compiled-in WIFI_SSID/PASSWORD defaults.
    # (Printing a comment here would be parsed as a bogus build flag and fail.)
    pass