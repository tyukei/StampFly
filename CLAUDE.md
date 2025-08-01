# StampFly - ESP32-S3ベースのドローン制御システム

## プロジェクト概要
StampFlyは、ESP32-S3 DevKit-C-1をベースとしたドローンの飛行制御システムです。400Hzの制御周期で動作し、IMU、ToF、気圧センサなどを使用してクアッドコプターの安定した飛行を実現しています。

## ハードウェア構成
- **メインマイコン**: ESP32-S3 DevKit-C-1
- **センサー**:
  - BMI270: 6軸IMU（加速度センサー・ジャイロスコープ）
  - VL53L3CX: ToF距離センサー
  - BMP280: 気圧センサー
  - BMM150: 磁気センサー
- **アクチュエーター**: 4個のブラシレスモーター（150kHz PWM制御）
- **通信**: 無線コントローラー（Ps3ライブラリ使用）
- **表示**: LED制御（FastLED）

## ファイル構成

### /src/
- **main.cpp**: メインエントリーポイント、セットアップとループ処理
- **flight_control.cpp/hpp**: 飛行制御のメイン処理（400Hz制御ループ）
- **imu.cpp/hpp**: IMU（BMI270）制御とMadgwick AHRS姿勢推定
- **sensor.cpp/hpp**: 各種センサー（気圧、磁気）の読み取り
- **tof.cpp/hpp**: ToF距離センサー制御
- **rc.cpp/hpp**: 無線コントローラー入力処理
- **pid.cpp/hpp**: PID制御器の実装
- **led.cpp/hpp**: LED制御とステータス表示
- **telemetry.cpp/hpp**: テレメトリーデータ処理
- **alt_kalman.cpp/hpp**: 高度制御用カルマンフィルター

### /lib/
- **MdgwickAHRS/**: 姿勢推定アルゴリズム
- **bmi270/**: BMI270 IMUセンサーライブラリ
- **bmm150/**: BMM150磁気センサーライブラリ
- **bmp280/**: BMP280気圧センサーライブラリ
- **vl53l3c/**: VL53L3CX ToFセンサーライブラリ

## ビルド設定
### platformio.ini
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
lib_deps = 
    fastled/FastLED
    tinyu-zhao/INA3221
```

## 制御システム

### 制御周期
- **メインループ**: 400Hz（2.5ms）
- **モーター出力**: PWM 150kHz、8bit分解能

### 飛行モード
1. **Stabilizeモード**: 姿勢自動制御
2. **Acroモード**: 角速度制御（姿勢保持なし）
3. **Altitude Holdモード**: 高度維持（開発中）

### PID制御パラメータ
- **Roll Rate**: Kp=0.6, Ti=0.7, Td=0.01
- **Pitch Rate**: Kp=0.75, Ti=0.7, Td=0.025
- **Yaw Rate**: Kp=1.5, Ti=2.5, Td=0.001

### モーターピン配置
- Front Left: GPIO5
- Front Right: GPIO42
- Rear Left: GPIO10
- Rear Right: GPIO41

## 開発・テスト

### ビルド
```bash
pio run
```

### アップロード
```bash
pio run --target upload
```

### シリアルモニター
```bash
pio device monitor
```

## 安全機能
- 衝撃検知による自動停止
- 電池電圧監視
- LED色によるステータス表示
- アーミング/ディスアーミング機能

## 注意事項
- 安全メガネの着用を推奨
- 電池の正しい接続を確認
- 初回飛行時はペアリングが必要
- 水平面での初期化が必要

## 開発者
設計: Kouhei Ito (2023~2024)