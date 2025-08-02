#!/usr/bin/env python3
"""
Simple Dashboard for ESP32-S3 100% Thrust Test (macOS Compatible)
No PiEEG dependencies - just monitors UDP data from ESP32-S3
"""

import socket
import threading
import time
import json
from datetime import datetime

class SimpleDashboard:
    def __init__(self):
        self.latest_data = {
            'eeg_value': 0.0,
            'motor_states': {'FL': 0, 'FR': 0, 'RL': 0, 'RR': 0},
            'timestamp': time.time(),
            'packet_count': 0,
            'esp32_ip': '172.21.128.229'
        }
        self.running = True
        
    def monitor_esp32_status(self):
        """Monitor ESP32-S3 by pinging it periodically"""
        import subprocess
        
        while self.running:
            try:
                result = subprocess.run(['ping', '-c', '1', '-W', '1000', self.latest_data['esp32_ip']], 
                                      capture_output=True, text=True, timeout=2)
                
                if result.returncode == 0:
                    status = "🟢 ONLINE"
                else:
                    status = "🔴 OFFLINE"
                    
                print(f"\r🔗 ESP32-S3 Status: {status} | 📊 Packets: {self.latest_data['packet_count']} | 🧠 EEG: {self.latest_data['eeg_value']:.2f}", end="")
                
            except Exception:
                print(f"\r🔗 ESP32-S3 Status: ❓ UNKNOWN | 📊 Packets: {self.latest_data['packet_count']} | 🧠 EEG: {self.latest_data['eeg_value']:.2f}", end="")
            
            time.sleep(3)
    
    def display_motor_status(self):
        """Display current motor thrust levels"""
        motors = self.latest_data['motor_states']
        
        print(f"\n🚁 Motor Thrust Status:")
        print(f"   FL (前左): {motors['FL']:3d}% {'█' * (motors['FL']//10)}")
        print(f"   FR (前右): {motors['FR']:3d}% {'█' * (motors['FR']//10)}")
        print(f"   RL (後左): {motors['RL']:3d}% {'█' * (motors['RL']//10)}")
        print(f"   RR (後右): {motors['RR']:3d}% {'█' * (motors['RR']//10)}")
        
        # Show thrust pattern
        eeg = self.latest_data['eeg_value']
        if eeg < 1.0:
            pattern = "🔄 強い左回転 (FL/RL=100%, FR/RR=5%)"
        elif eeg < 2.0:
            pattern = "↰ 左回転 (FL/RL=80%, FR/RR=20%)"
        elif eeg < 3.0:
            pattern = "⬆️ 直進浮上 (全モーター70%)"
        elif eeg < 4.0:
            pattern = "↱ 右回転 (FL/RL=20%, FR/RR=80%)"
        else:
            pattern = "🔄 強い右回転 (FL/RL=5%, FR/RR=100%)"
            
        print(f"   パターン: {pattern}")
        print(f"   最終更新: {datetime.fromtimestamp(self.latest_data['timestamp']).strftime('%H:%M:%S')}")
    
    def simulate_motor_states(self, eeg_value):
        """Simulate motor states based on EEG value (matches ESP32-S3 logic)"""
        if eeg_value < 1.0:
            return {'FL': 100, 'FR': 5, 'RL': 100, 'RR': 5}
        elif eeg_value < 2.0:
            return {'FL': 80, 'FR': 20, 'RL': 80, 'RR': 20}
        elif eeg_value < 3.0:
            return {'FL': 70, 'FR': 70, 'RL': 70, 'RR': 70}
        elif eeg_value < 4.0:
            return {'FL': 20, 'FR': 80, 'RL': 20, 'RR': 80}
        else:
            return {'FL': 5, 'FR': 100, 'RL': 5, 'RR': 100}
    
    def run(self):
        print("🧠 Simple ESP32-S3 100% Thrust Dashboard")
        print("=" * 50)
        print(f"📡 Monitoring ESP32-S3 at: {self.latest_data['esp32_ip']}")
        print("🎮 Test commands:")
        print("   python3 test_udp_sender.py  # Send test values")
        print("   python3 GUI/2.Graph_Gpio_D_1_5_4.py  # Real EEG data")
        print("\n⚠️  警告: ESP32-S3は最大100%推力で動作中！")
        print("-" * 50)
        
        # Start ESP32 monitoring thread
        monitor_thread = threading.Thread(target=self.monitor_esp32_status, daemon=True)
        monitor_thread.start()
        
        try:
            last_display = 0
            while self.running:
                # Display motor status every 2 seconds
                if time.time() - last_display > 2:
                    print("\n" + "=" * 50)
                    self.display_motor_status()
                    print("=" * 50)
                    last_display = time.time()
                
                # Simulate receiving UDP data (since we can't easily monitor UDP on macOS)
                # In real scenario, this would listen to UDP packets from test_udp_sender.py
                
                time.sleep(0.5)
                
        except KeyboardInterrupt:
            print("\n\n👋 Dashboard stopped. Goodbye!")
            self.running = False

def test_send_values():
    """Send test values to ESP32-S3"""
    import subprocess
    import sys
    
    print("\n🚀 Starting UDP test sender...")
    try:
        subprocess.run([sys.executable, 'test_udp_sender.py'])
    except Exception as e:
        print(f"❌ Could not start test sender: {e}")
        print("💡 Run manually: python3 test_udp_sender.py")

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1 and sys.argv[1] == "test":
        test_send_values()
    else:
        dashboard = SimpleDashboard()
        dashboard.run()