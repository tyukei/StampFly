#!/usr/bin/env python3
"""
Web Dashboard for ESP32-S3 100% Thrust Test (macOS Compatible)
Simple HTTP server with HTML UI - No Flask dependencies
"""

import http.server
import socketserver
import json
import threading
import time
import socket
import subprocess
from datetime import datetime
from urllib.parse import parse_qs

class DashboardHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, dashboard_instance=None, **kwargs):
        self.dashboard = dashboard_instance
        super().__init__(*args, **kwargs)
    
    def do_GET(self):
        if self.path == '/':
            self.serve_dashboard()
        elif self.path == '/api/status':
            self.serve_status_api()
        elif self.path == '/api/send_test':
            self.send_test_values()
        else:
            super().do_GET()
    
    def do_POST(self):
        if self.path == '/api/send_eeg':
            self.handle_eeg_send()
        else:
            self.send_error(404)
    
    def serve_dashboard(self):
        html_content = """
<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-S3 100% Thrust Dashboard</title>
    <style>
        body { 
            font-family: 'Courier New', monospace; 
            background: #0a0a0a; 
            color: #00ff00; 
            margin: 0; 
            padding: 20px; 
        }
        .container { max-width: 1200px; margin: 0 auto; }
        .header { 
            text-align: center; 
            border: 2px solid #00ff00; 
            padding: 20px; 
            margin-bottom: 20px;
            background: #001100;
        }
        .warning { 
            background: #330000; 
            border: 2px solid #ff0000; 
            color: #ff4444; 
            padding: 15px; 
            margin: 20px 0;
            text-align: center;
            font-weight: bold;
        }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
        .panel { 
            border: 2px solid #00ff00; 
            padding: 20px; 
            background: #001100;
        }
        .motor-grid { 
            display: grid; 
            grid-template-columns: 1fr 1fr; 
            gap: 10px; 
            margin: 20px 0;
        }
        .motor { 
            border: 1px solid #00ff00; 
            padding: 15px; 
            text-align: center;
            background: #002200;
        }
        .motor.active { background: #004400; border-color: #00ff88; }
        .thrust-bar { 
            height: 20px; 
            background: #003300; 
            border: 1px solid #00ff00; 
            margin: 10px 0;
            position: relative;
        }
        .thrust-fill { 
            height: 100%; 
            background: linear-gradient(90deg, #00ff00, #ffff00, #ff0000); 
            transition: width 0.3s;
        }
        .controls { margin: 20px 0; }
        .btn { 
            background: #003300; 
            border: 2px solid #00ff00; 
            color: #00ff00; 
            padding: 10px 20px; 
            margin: 5px; 
            cursor: pointer;
            font-family: inherit;
        }
        .btn:hover { background: #004400; }
        .status { font-size: 18px; margin: 10px 0; }
        .eeg-input { 
            background: #001100; 
            border: 2px solid #00ff00; 
            color: #00ff00; 
            padding: 10px; 
            font-family: inherit;
            width: 100px;
        }
        .pattern-display {
            font-size: 24px;
            text-align: center;
            padding: 20px;
            margin: 20px 0;
            border: 2px solid #00ff00;
            background: #002200;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🚁 ESP32-S3 100% THRUST DASHBOARD 🚁</h1>
            <div class="status" id="esp32-status">🔗 ESP32-S3: Checking...</div>
            <div class="status" id="packet-count">📊 Packets: 0</div>
        </div>
        
        <div class="warning">
            ⚠️ 警告: ESP32-S3は最大100%推力で動作中！プロペラが外れていることを確認してください！ ⚠️
        </div>
        
        <div class="grid">
            <div class="panel">
                <h2>🎮 Manual Control</h2>
                <div class="controls">
                    <label>EEG Value: </label>
                    <input type="number" id="eeg-input" class="eeg-input" min="0" max="5" step="0.1" value="2.5">
                    <button class="btn" onclick="sendEEGValue()">Send</button>
                </div>
                <div class="controls">
                    <button class="btn" onclick="sendTestPattern()">🔄 Run Test Pattern</button>
                    <button class="btn" onclick="emergencyStop()">🛑 Emergency Stop</button>
                </div>
                <div class="controls">
                    <h3>Quick Values:</h3>
                    <button class="btn" onclick="quickSend(0.5)">強い左回転 (0.5)</button>
                    <button class="btn" onclick="quickSend(1.5)">左回転 (1.5)</button>
                    <button class="btn" onclick="quickSend(2.5)">直進浮上 (2.5)</button>
                    <button class="btn" onclick="quickSend(3.5)">右回転 (3.5)</button>
                    <button class="btn" onclick="quickSend(4.5)">強い右回転 (4.5)</button>
                </div>
            </div>
            
            <div class="panel">
                <h2>🧠 Current State</h2>
                <div class="status">EEG Value: <span id="current-eeg">0.00</span></div>
                <div class="pattern-display" id="current-pattern">待機中...</div>
                <div class="status">Last Update: <span id="last-update">--:--:--</span></div>
            </div>
        </div>
        
        <div class="panel">
            <h2>🚁 Motor Thrust Status</h2>
            <div class="motor-grid">
                <div class="motor" id="motor-fl">
                    <h3>FL (前左)</h3>
                    <div class="thrust-bar">
                        <div class="thrust-fill" id="thrust-fl" style="width: 0%"></div>
                    </div>
                    <div id="value-fl">0%</div>
                </div>
                <div class="motor" id="motor-fr">
                    <h3>FR (前右)</h3>
                    <div class="thrust-bar">
                        <div class="thrust-fill" id="thrust-fr" style="width: 0%"></div>
                    </div>
                    <div id="value-fr">0%</div>
                </div>
                <div class="motor" id="motor-rl">
                    <h3>RL (後左)</h3>
                    <div class="thrust-bar">
                        <div class="thrust-fill" id="thrust-rl" style="width: 0%"></div>
                    </div>
                    <div id="value-rl">0%</div>
                </div>
                <div class="motor" id="motor-rr">
                    <h3>RR (後右)</h3>
                    <div class="thrust-bar">
                        <div class="thrust-fill" id="thrust-rr" style="width: 0%"></div>
                    </div>
                    <div id="value-rr">0%</div>
                </div>
            </div>
        </div>
    </div>

    <script>
        const ESP32_IP = '172.21.128.229';
        
        function updateMotorDisplay(eegValue) {
            let motors = getMotorValues(eegValue);
            let pattern = getPattern(eegValue);
            
            // Update motor displays
            ['fl', 'fr', 'rl', 'rr'].forEach(motor => {
                const value = motors[motor.toUpperCase()];
                document.getElementById(`thrust-${motor}`).style.width = value + '%';
                document.getElementById(`value-${motor}`).textContent = value + '%';
                
                // Highlight active motors
                const motorDiv = document.getElementById(`motor-${motor}`);
                if (value > 50) {
                    motorDiv.classList.add('active');
                } else {
                    motorDiv.classList.remove('active');
                }
            });
            
            // Update current state
            document.getElementById('current-eeg').textContent = eegValue.toFixed(2);
            document.getElementById('current-pattern').textContent = pattern;
            document.getElementById('last-update').textContent = new Date().toLocaleTimeString();
        }
        
        function getMotorValues(eegValue) {
            if (eegValue < 1.0) {
                return {FL: 100, FR: 5, RL: 100, RR: 5};
            } else if (eegValue < 2.0) {
                return {FL: 80, FR: 20, RL: 80, RR: 20};
            } else if (eegValue < 3.0) {
                return {FL: 70, FR: 70, RL: 70, RR: 70};
            } else if (eegValue < 4.0) {
                return {FL: 20, FR: 80, RL: 20, RR: 80};
            } else {
                return {FL: 5, FR: 100, RL: 5, RR: 100};
            }
        }
        
        function getPattern(eegValue) {
            if (eegValue < 1.0) return "🔄 強い左回転 (FL/RL=100%, FR/RR=5%)";
            if (eegValue < 2.0) return "↰ 左回転 (FL/RL=80%, FR/RR=20%)";
            if (eegValue < 3.0) return "⬆️ 直進浮上 (全モーター70%)";
            if (eegValue < 4.0) return "↱ 右回転 (FL/RL=20%, FR/RR=80%)";
            return "🔄 強い右回転 (FL/RL=5%, FR/RR=100%)";
        }
        
        async function sendEEGValue() {
            const value = parseFloat(document.getElementById('eeg-input').value);
            try {
                const response = await fetch('/api/send_eeg', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({eeg_value: value})
                });
                if (response.ok) {
                    updateMotorDisplay(value);
                    console.log(`Sent EEG value: ${value}`);
                }
            } catch (error) {
                console.error('Error sending EEG value:', error);
            }
        }
        
        function quickSend(value) {
            document.getElementById('eeg-input').value = value;
            sendEEGValue();
        }
        
        async function sendTestPattern() {
            const testValues = [0.5, 1.5, 2.5, 3.5, 4.5];
            for (let value of testValues) {
                document.getElementById('eeg-input').value = value;
                await sendEEGValue();
                await new Promise(resolve => setTimeout(resolve, 3000)); // Wait 3 seconds
            }
        }
        
        function emergencyStop() {
            document.getElementById('eeg-input').value = 10; // Out of range = minimum thrust
            sendEEGValue();
        }
        
        // Update status periodically
        setInterval(async function() {
            try {
                const response = await fetch('/api/status');
                const status = await response.json();
                document.getElementById('esp32-status').textContent = 
                    `🔗 ESP32-S3: ${status.online ? '🟢 ONLINE' : '🔴 OFFLINE'}`;
                document.getElementById('packet-count').textContent = 
                    `📊 Packets: ${status.packet_count}`;
            } catch (error) {
                document.getElementById('esp32-status').textContent = '🔗 ESP32-S3: ❓ UNKNOWN';
            }
        }, 3000);
        
        // Initialize with default values
        updateMotorDisplay(2.5);
    </script>
</body>
</html>
        """
        
        self.send_response(200)
        self.send_header('Content-type', 'text/html; charset=utf-8')
        self.end_headers()
        self.wfile.write(html_content.encode('utf-8'))
    
    def serve_status_api(self):
        # Check ESP32 status
        try:
            result = subprocess.run(['ping', '-c', '1', '-W', '1000', '172.21.128.229'], 
                                  capture_output=True, timeout=2)
            online = result.returncode == 0
        except:
            online = False
        
        status = {
            'online': online,
            'packet_count': getattr(self.dashboard, 'packet_count', 0),
            'timestamp': time.time()
        }
        
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(status).encode('utf-8'))
    
    def handle_eeg_send(self):
        content_length = int(self.headers.get('Content-Length', 0))
        post_data = self.rfile.read(content_length)
        
        try:
            data = json.loads(post_data.decode('utf-8'))
            eeg_value = data.get('eeg_value', 0)
            
            # Send UDP packet to ESP32-S3
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            message = str(eeg_value)
            sock.sendto(message.encode(), ('172.21.128.229', 4210))
            sock.close()
            
            if hasattr(self.dashboard, 'packet_count'):
                self.dashboard.packet_count += 1
            
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({'status': 'success', 'sent_value': eeg_value}).encode('utf-8'))
            
            print(f"📤 Sent EEG value {eeg_value} to ESP32-S3")
            
        except Exception as e:
            self.send_response(500)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({'status': 'error', 'message': str(e)}).encode('utf-8'))

class WebDashboard:
    def __init__(self, port=8080):
        self.port = port
        self.packet_count = 0
        
    def run(self):
        def handler(*args, **kwargs):
            return DashboardHandler(*args, dashboard_instance=self, **kwargs)
        
        with socketserver.TCPServer(("", self.port), handler) as httpd:
            print("🌐 ESP32-S3 100% Thrust Web Dashboard")
            print("=" * 50)
            print(f"🔗 Web UI: http://localhost:{self.port}")
            print(f"📡 ESP32-S3: 172.21.128.229:4210")
            print("⚠️  警告: 100%推力で動作中！")
            print("=" * 50)
            print("Ctrl+C to stop")
            
            try:
                httpd.serve_forever()
            except KeyboardInterrupt:
                print("\n\n👋 Web dashboard stopped. Goodbye!")

if __name__ == "__main__":
    import sys
    
    port = 8080
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            print("Invalid port number, using default 8080")
    
    dashboard = WebDashboard(port)
    dashboard.run()