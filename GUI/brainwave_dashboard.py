#!/usr/bin/env python3
"""
Brainwave Dashboard for PiEEG - Fixed Version
Compatible with both Raspberry Pi and macOS
"""

import http.server
import socketserver
import json
import threading
import time
import os
from datetime import datetime

class BrainwaveDashboardHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, dashboard_instance=None, **kwargs):
        self.dashboard = dashboard_instance
        super().__init__(*args, **kwargs)
    
    def do_GET(self):
        if self.path == '/':
            self.serve_brainwave_dashboard()
        elif self.path == '/api/brainwave_data':
            self.serve_brainwave_api()
        else:
            super().do_GET()
    
    def serve_brainwave_dashboard(self):
        html_content = """
<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>🧠 PiEEG Brainwave Dashboard</title>
    <style>
        body { 
            font-family: 'Arial', sans-serif; 
            background: linear-gradient(135deg, #0f0f23, #1a1a2e, #16213e); 
            color: #ffffff; 
            margin: 0; 
            padding: 20px; 
        }
        .container { max-width: 1400px; margin: 0 auto; }
        .header { 
            text-align: center; 
            border: 3px solid #00d4ff; 
            padding: 25px; 
            margin-bottom: 25px;
            background: linear-gradient(45deg, #001122, #002244);
            border-radius: 15px;
            box-shadow: 0 0 30px rgba(0, 212, 255, 0.3);
        }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 25px; margin-bottom: 25px; }
        .full-width { grid-column: 1 / -1; }
        .panel { 
            border: 2px solid #00d4ff; 
            padding: 25px; 
            background: linear-gradient(135deg, #001122, #002244);
            border-radius: 12px;
            box-shadow: 0 0 20px rgba(0, 212, 255, 0.2);
        }
        .wave-item {
            display: flex;
            align-items: center;
            margin: 15px 0;
            padding: 15px;
            background: rgba(0, 212, 255, 0.1);
            border-radius: 8px;
            border-left: 4px solid;
        }
        .wave-theta { border-left-color: #ff6b6b; }
        .wave-alpha { border-left-color: #4ecdc4; }
        .wave-beta { border-left-color: #45b7d1; }
        .wave-gamma { border-left-color: #f9ca24; }
        .wave-name { 
            font-weight: bold; 
            width: 80px; 
            font-size: 18px;
        }
        .wave-bar-container { 
            flex: 1; 
            height: 25px; 
            background: #001122; 
            border-radius: 12px; 
            margin: 0 15px;
            position: relative;
            overflow: hidden;
        }
        .wave-bar { 
            height: 100%; 
            border-radius: 12px; 
            transition: width 0.5s ease;
            position: relative;
        }
        .wave-bar.theta { background: linear-gradient(90deg, #ff6b6b, #ff8e8e); }
        .wave-bar.alpha { background: linear-gradient(90deg, #4ecdc4, #6ed4d2); }
        .wave-bar.beta { background: linear-gradient(90deg, #45b7d1, #67c3d6); }
        .wave-bar.gamma { background: linear-gradient(90deg, #f9ca24, #fdd648); }
        .wave-value { 
            font-weight: bold; 
            font-size: 16px;
            width: 80px;
            text-align: right;
        }
        .dominant-wave {
            text-align: center;
            font-size: 32px;
            font-weight: bold;
            padding: 30px;
            margin: 20px 0;
            border-radius: 12px;
            box-shadow: 0 0 25px rgba(255, 255, 255, 0.1);
        }
        .dominant-theta { background: linear-gradient(45deg, #ff6b6b, #ff8e8e); color: white; }
        .dominant-alpha { background: linear-gradient(45deg, #4ecdc4, #6ed4d2); color: white; }
        .dominant-beta { background: linear-gradient(45deg, #45b7d1, #67c3d6); color: white; }
        .dominant-gamma { background: linear-gradient(45deg, #f9ca24, #fdd648); color: black; }
        .status-info {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            font-size: 14px;
        }
        .status-item {
            padding: 10px;
            background: rgba(0, 212, 255, 0.1);
            border-radius: 6px;
            border-left: 3px solid #00d4ff;
        }
        .chart-container {
            height: 300px;
            background: rgba(0, 20, 40, 0.3);
            border-radius: 8px;
            display: flex;
            align-items: center;
            justify-content: center;
            color: #888;
            font-size: 18px;
        }
        .refresh-button {
            background: linear-gradient(45deg, #00d4ff, #0099cc);
            border: none;
            color: white;
            padding: 12px 25px;
            border-radius: 6px;
            cursor: pointer;
            font-size: 16px;
            margin: 10px;
        }
        .refresh-button:hover { background: linear-gradient(45deg, #0099cc, #0077aa); }
        .connection-status {
            padding: 10px 20px;
            border-radius: 20px;
            font-weight: bold;
            margin: 10px;
        }
        .connected { background: #27ae60; color: white; }
        .disconnected { background: #e74c3c; color: white; }
        .waiting { background: #f39c12; color: white; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🧠 PiEEG Brainwave Dashboard</h1>
            <div id="connection-status" class="connection-status waiting">⏳ データ待機中...</div>
            <button class="refresh-button" onclick="refreshData()">🔄 手動更新</button>
        </div>
        
        <div class="grid">
            <div class="panel">
                <h2>📊 脳波パワー</h2>
                <div class="wave-item wave-theta">
                    <div class="wave-name">θ Theta</div>
                    <div class="wave-bar-container">
                        <div class="wave-bar theta" id="theta-bar" style="width: 0%"></div>
                    </div>
                    <div class="wave-value" id="theta-value">0.000000</div>
                </div>
                <div class="wave-item wave-alpha">
                    <div class="wave-name">α Alpha</div>
                    <div class="wave-bar-container">
                        <div class="wave-bar alpha" id="alpha-bar" style="width: 0%"></div>
                    </div>
                    <div class="wave-value" id="alpha-value">0.000000</div>
                </div>
                <div class="wave-item wave-beta">
                    <div class="wave-name">β Beta</div>
                    <div class="wave-bar-container">
                        <div class="wave-bar beta" id="beta-bar" style="width: 0%"></div>
                    </div>
                    <div class="wave-value" id="beta-value">0.000000</div>
                </div>
                <div class="wave-item wave-gamma">
                    <div class="wave-name">γ Gamma</div>
                    <div class="wave-bar-container">
                        <div class="wave-bar gamma" id="gamma-bar" style="width: 0%"></div>
                    </div>
                    <div class="wave-value" id="gamma-value">0.000000</div>
                </div>
            </div>
            
            <div class="panel">
                <h2>🎯 優勢な脳波</h2>
                <div class="dominant-wave" id="dominant-display">
                    <div id="dominant-wave">待機中...</div>
                    <div style="font-size: 16px; margin-top: 10px;" id="dominant-desc">
                        データを受信中です...
                    </div>
                </div>
                
                <div class="status-info">
                    <div class="status-item">
                        <strong>📅 最終更新</strong><br>
                        <span id="last-update">--:--:--</span>
                    </div>
                    <div class="status-item">
                        <strong>📈 データ数</strong><br>
                        <span id="data-count">0</span>
                    </div>
                    <div class="status-item">
                        <strong>🔗 接続状態</strong><br>
                        <span id="connection-info">待機中</span>
                    </div>
                    <div class="status-item">
                        <strong>⚡ アクティブ状態</strong><br>
                        <span id="active-state">スタンバイ</span>
                    </div>
                </div>
            </div>
        </div>
        
        <div class="panel full-width">
            <h2>📈 リアルタイム波形 (予定)</h2>
            <div class="chart-container">
                💡 リアルタイム脳波チャートを実装予定
                <br>現在は上記の数値データで確認してください
            </div>
        </div>
    </div>

    <script>
        let dataCount = 0;
        let lastUpdateTime = 0;
        
        const waveDescriptions = {
            'theta': '深いリラックス・瞑想状態',
            'alpha': 'リラックス・集中準備状態', 
            'beta': '通常の覚醒・思考状態',
            'gamma': '高度な集中・認知状態'
        };
        
        function updateBrainwaveDisplay(data) {
            if (!data) return;
            
            // 各脳波の値を更新
            const waves = ['theta', 'alpha', 'beta', 'gamma'];
            let maxValue = 0;
            
            waves.forEach(wave => {
                const value = data[wave + '_power'] || 0;
                maxValue = Math.max(maxValue, value);
            });
            
            // 正規化して表示（最大値を100%として）
            waves.forEach(wave => {
                const value = data[wave + '_power'] || 0;
                const percentage = maxValue > 0 ? (value / maxValue) * 100 : 0;
                
                document.getElementById(wave + '-bar').style.width = percentage + '%';
                document.getElementById(wave + '-value').textContent = value.toFixed(6);
            });
            
            // 優勢な脳波を表示
            const dominantWave = data.dominant_wave || 'unknown';
            const dominantElement = document.getElementById('dominant-display');
            
            dominantElement.className = 'dominant-wave dominant-' + dominantWave;
            document.getElementById('dominant-wave').textContent = 
                dominantWave.charAt(0).toUpperCase() + dominantWave.slice(1) + ' 波';
            document.getElementById('dominant-desc').textContent = 
                waveDescriptions[dominantWave] || '状態を解析中...';
            
            // ステータス情報を更新
            document.getElementById('last-update').textContent = 
                new Date(data.timestamp * 1000).toLocaleTimeString();
            document.getElementById('data-count').textContent = ++dataCount;
            document.getElementById('connection-info').textContent = '接続中';
            document.getElementById('active-state').textContent = 'アクティブ';
            
            // 接続ステータス更新
            const statusElement = document.getElementById('connection-status');
            statusElement.textContent = '🟢 データ受信中';
            statusElement.className = 'connection-status connected';
            
            lastUpdateTime = Date.now();
        }
        
        async function fetchBrainwaveData() {
            try {
                const response = await fetch('/api/brainwave_data');
                if (response.ok) {
                    const data = await response.json();
                    updateBrainwaveDisplay(data);
                } else {
                    throw new Error('データ取得失敗');
                }
            } catch (error) {
                console.log('データ取得エラー:', error);
                
                // 5秒以上データが来ない場合は切断状態とする
                if (Date.now() - lastUpdateTime > 5000) {
                    const statusElement = document.getElementById('connection-status');
                    statusElement.textContent = '🔴 データなし';
                    statusElement.className = 'connection-status disconnected';
                    document.getElementById('connection-info').textContent = '切断';
                    document.getElementById('active-state').textContent = 'スタンバイ';
                }
            }
        }
        
        function refreshData() {
            fetchBrainwaveData();
        }
        
        // 定期的にデータを更新（500ms間隔）
        setInterval(fetchBrainwaveData, 500);
        
        // 初回データ取得
        fetchBrainwaveData();
    </script>
</body>
</html>
        """
        
        self.send_response(200)
        self.send_header('Content-type', 'text/html; charset=utf-8')
        self.end_headers()
        self.wfile.write(html_content.encode('utf-8'))
    
    def serve_brainwave_api(self):
        # 脳波データファイルを読み取り
        data_file = '/tmp/latest_eeg_data.json'
        
        try:
            if os.path.exists(data_file):
                with open(data_file, 'r') as f:
                    data = json.load(f)
            else:
                # ファイルが存在しない場合のデフォルトデータ
                data = {
                    'theta_power': 0.0,
                    'alpha_power': 0.0,
                    'beta_power': 0.0,
                    'gamma_power': 0.0,
                    'dominant_wave': 'unknown',
                    'timestamp': time.time()
                }
        except Exception as e:
            print(f"データ読み取りエラー: {e}")
            data = {
                'theta_power': 0.0,
                'alpha_power': 0.0,
                'beta_power': 0.0,
                'gamma_power': 0.0,
                'dominant_wave': 'error',
                'timestamp': time.time(),
                'error': str(e)
            }
        
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode('utf-8'))

class BrainwaveDashboard:
    def __init__(self, port=8081):
        self.port = port
        
    def run(self):
        def handler(*args, **kwargs):
            return BrainwaveDashboardHandler(*args, dashboard_instance=self, **kwargs)
        
        with socketserver.TCPServer(("", self.port), handler) as httpd:
            print("🧠 PiEEG Brainwave Dashboard")
            print("=" * 50)
            print(f"🌐 Web UI: http://localhost:{self.port}")
            print(f"📁 Data File: /tmp/latest_eeg_data.json")
            print("📊 Real-time brainwave visualization")
            print("=" * 50)
            print("Ctrl+C to stop")
            
            try:
                httpd.serve_forever()
            except KeyboardInterrupt:
                print("\n\n👋 Brainwave dashboard stopped. Goodbye!")

if __name__ == "__main__":
    import sys
    
    port = 8081
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            print("Invalid port number, using default 8081")
    
    dashboard = BrainwaveDashboard(port)
    dashboard.run()