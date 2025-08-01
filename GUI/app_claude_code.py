#!/usr/bin/env python3
"""
PiEEG Brainwave Dashboard with Claude Code Integration
A dynamic web dashboard for real-time brainwave visualization that saves data for Claude Code analysis
"""

import asyncio
import json
import time
import threading
from datetime import datetime
from flask import Flask, render_template, jsonify, request
from flask_socketio import SocketIO, emit
import paho.mqtt.client as mqtt
from collections import deque
import numpy as np
import os
import re
from typing import Dict, List, Optional
from pathlib import Path

app = Flask(__name__)
app.config['SECRET_KEY'] = 'pieeg_dashboard_secret'
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# Create data directory
DATA_DIR = Path(__file__).parent / "brainwave_data"
DATA_DIR.mkdir(exist_ok=True)

# Global data storage
brainwave_data = deque(maxlen=1000)  # Store last 1000 readings
channel_data = deque(maxlen=100)  # Store last 100 channel readings
current_state = {
    'theta_power': 0,
    'alpha_power': 0,
    'beta_power': 0,
    'gamma_power': 0,
    'dominant_wave': 'alpha',
    'timestamp': time.time()
}
current_channels = {
    'channels': {},
    'timestamp': time.time()
}

# AI Analysis storage
ai_insights = {
    'current_analysis': 'Run "claude dashboard/analyze_brainwaves.py" to get AI analysis',
    'recommendations': ['Save some brainwave data first', 'Then run Claude Code for analysis'],
    'mood_assessment': 'Pending analysis',
    'stress_level': 0,
    'focus_level': 0,
    'relaxation_level': 0,
    'last_updated': time.time()
}

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "pieeg/m5stamp/commands"
MQTT_CHANNEL_TOPIC = "pieeg/channels/data"

class BrainwaveAnalyzer:
    def __init__(self):
        self.history = deque(maxlen=100)
        self.recording_data = []
        self.is_recording = False
        
    def analyze_patterns(self, data: Dict) -> Dict:
        """Analyze brainwave patterns and generate insights"""
        self.history.append(data)
        
        if len(self.history) < 10:
            return {'analysis': 'Collecting data...', 'confidence': 0}
        
        # Calculate trends
        recent_data = list(self.history)[-10:]
        
        theta_trend = np.mean([d['theta_power'] for d in recent_data])
        alpha_trend = np.mean([d['alpha_power'] for d in recent_data])
        beta_trend = np.mean([d['beta_power'] for d in recent_data])
        gamma_trend = np.mean([d['gamma_power'] for d in recent_data])
        
        # Generate analysis
        analysis = {
            'theta_trend': theta_trend,
            'alpha_trend': alpha_trend,
            'beta_trend': beta_trend,
            'gamma_trend': gamma_trend,
            'stress_level': min(100, beta_trend * 1000),
            'focus_level': min(100, (beta_trend + gamma_trend) * 500),
            'relaxation_level': min(100, (theta_trend + alpha_trend) * 500)
        }
        
        return analysis
    
    def save_data_for_analysis(self, data: List[Dict], filename: str = "latest_brainwave_data.json"):
        """Save brainwave data to file for Claude Code analysis"""
        filepath = DATA_DIR / filename
        
        # Prepare data with metadata
        save_data = {
            'timestamp': datetime.now().isoformat(),
            'data_points': len(data),
            'duration_seconds': len(data) * 0.1 if data else 0,  # Assuming 10Hz sampling
            'brainwave_data': data,
            'statistics': self.calculate_statistics(data) if data else {}
        }
        
        with open(filepath, 'w') as f:
            json.dump(save_data, f, indent=2)
        
        return filepath
    
    def calculate_statistics(self, data: List[Dict]) -> Dict:
        """Calculate statistics for the data"""
        if not data:
            return {}
        
        stats = {
            'theta': {
                'mean': np.mean([d.get('theta_power', 0) for d in data]),
                'std': np.std([d.get('theta_power', 0) for d in data]),
                'min': np.min([d.get('theta_power', 0) for d in data]),
                'max': np.max([d.get('theta_power', 0) for d in data])
            },
            'alpha': {
                'mean': np.mean([d.get('alpha_power', 0) for d in data]),
                'std': np.std([d.get('alpha_power', 0) for d in data]),
                'min': np.min([d.get('alpha_power', 0) for d in data]),
                'max': np.max([d.get('alpha_power', 0) for d in data])
            },
            'beta': {
                'mean': np.mean([d.get('beta_power', 0) for d in data]),
                'std': np.std([d.get('beta_power', 0) for d in data]),
                'min': np.min([d.get('beta_power', 0) for d in data]),
                'max': np.max([d.get('beta_power', 0) for d in data])
            },
            'gamma': {
                'mean': np.mean([d.get('gamma_power', 0) for d in data]),
                'std': np.std([d.get('gamma_power', 0) for d in data]),
                'min': np.min([d.get('gamma_power', 0) for d in data]),
                'max': np.max([d.get('gamma_power', 0) for d in data])
            }
        }
        
        # Find dominant wave
        means = {
            'theta': stats['theta']['mean'],
            'alpha': stats['alpha']['mean'],
            'beta': stats['beta']['mean'],
            'gamma': stats['gamma']['mean']
        }
        stats['dominant_wave'] = max(means, key=means.get)
        
        return stats

analyzer = BrainwaveAnalyzer()

def load_claude_analysis():
    """Load Claude Code analysis from file if it exists"""
    analysis_file = DATA_DIR / "claude_analysis.json"
    if analysis_file.exists():
        try:
            with open(analysis_file, 'r') as f:
                return json.load(f)
        except:
            pass
    return None

def on_mqtt_connect(client, userdata, flags, rc, properties=None):
    """MQTT connection callback"""
    print(f"Connected to MQTT broker with result code {rc}")
    client.subscribe(MQTT_TOPIC)
    client.subscribe(MQTT_CHANNEL_TOPIC)
    print(f"Subscribed to topics: {MQTT_TOPIC} and {MQTT_CHANNEL_TOPIC}")
    socketio.emit('mqtt_status', {'connected': True})

def on_mqtt_message(client, userdata, msg):
    """Handle incoming MQTT messages"""
    try:
        data = json.loads(msg.payload.decode())
        
        # Update current state
        current_state.update(data)
        current_state['timestamp'] = time.time()
        
        # Store in history
        brainwave_data.append(data)
        
        # If recording, add to recording data
        if analyzer.is_recording:
            analyzer.recording_data.append(data)
        
        # Analyze patterns
        analysis = analyzer.analyze_patterns(data)
        
        # Update AI insights with analysis
        ai_insights['stress_level'] = analysis.get('stress_level', 0)
        ai_insights['focus_level'] = analysis.get('focus_level', 0)
        ai_insights['relaxation_level'] = analysis.get('relaxation_level', 0)
        
        # Emit real-time data to clients
        socketio.emit('brainwave_data', {
            'current': current_state,
            'analysis': analysis,
            'timestamp': datetime.now().isoformat()
        })
        
        # Auto-save data every 100 readings
        if len(brainwave_data) % 100 == 0:
            threading.Thread(target=save_latest_data, daemon=True).start()
        
    except Exception as e:
        print(f"Error processing MQTT message: {e}")

def save_latest_data():
    """Save the latest brainwave data for Claude Code analysis"""
    try:
        recent_data = list(brainwave_data)[-100:] if len(brainwave_data) >= 100 else list(brainwave_data)
        filepath = analyzer.save_data_for_analysis(recent_data)
        print(f"Saved brainwave data to {filepath}")
        
        # Check if Claude analysis exists
        claude_analysis = load_claude_analysis()
        if claude_analysis:
            ai_insights.update(claude_analysis)
            ai_insights['last_updated'] = time.time()
            socketio.emit('ai_analysis', ai_insights)
    except Exception as e:
        print(f"Error saving data: {e}")

# Initialize MQTT client
try:
    mqtt_client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
except:
    # Fallback for older paho-mqtt versions
    mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message

# Routes
@app.route('/')
def index():
    """Main dashboard page"""
    return render_template('dashboard.html')

@app.route('/api/current')
def api_current():
    """Get current brainwave state"""
    return jsonify(current_state)

@app.route('/api/history')
def api_history():
    """Get brainwave history"""
    return jsonify(list(brainwave_data)[-100:])

@app.route('/api/ai-analysis')
def api_ai_analysis():
    """Get AI analysis"""
    # Try to load latest Claude analysis
    claude_analysis = load_claude_analysis()
    if claude_analysis:
        ai_insights.update(claude_analysis)
    return jsonify(ai_insights)

@app.route('/api/save-for-analysis', methods=['POST'])
def api_save_for_analysis():
    """Save current data for Claude Code analysis"""
    try:
        recent_data = list(brainwave_data)[-200:] if len(brainwave_data) >= 200 else list(brainwave_data)
        filepath = analyzer.save_data_for_analysis(recent_data)
        
        instructions = {
            'status': 'saved',
            'filepath': str(filepath),
            'instructions': [
                'Data saved! Now run Claude Code to analyze:',
                f'claude dashboard/analyze_brainwaves.py',
                'Or for a custom analysis:',
                f'claude "Analyze the brainwave data in {filepath}"'
            ]
        }
        
        return jsonify(instructions)
    except Exception as e:
        return jsonify({'status': 'error', 'message': str(e)}), 500

@app.route('/api/start-recording', methods=['POST'])
def api_start_recording():
    """Start recording brainwave data"""
    analyzer.is_recording = True
    analyzer.recording_data = []
    return jsonify({'status': 'recording_started'})

@app.route('/api/stop-recording', methods=['POST'])
def api_stop_recording():
    """Stop recording and save data"""
    analyzer.is_recording = False
    
    if analyzer.recording_data:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"recording_{timestamp}.json"
        filepath = analyzer.save_data_for_analysis(analyzer.recording_data, filename)
        
        return jsonify({
            'status': 'recording_saved',
            'filepath': str(filepath),
            'data_points': len(analyzer.recording_data)
        })
    else:
        return jsonify({'status': 'no_data'})

@socketio.on('connect')
def handle_connect():
    """Handle client connection"""
    print('Client connected')
    emit('current_state', current_state)
    emit('ai_analysis', ai_insights)

@socketio.on('disconnect')
def handle_disconnect():
    """Handle client disconnection"""
    print('Client disconnected')

@socketio.on('request_analysis')
def handle_request_analysis():
    """Handle analysis request from client"""
    # Save data and provide instructions
    recent_data = list(brainwave_data)[-200:] if len(brainwave_data) >= 200 else list(brainwave_data)
    filepath = analyzer.save_data_for_analysis(recent_data)
    
    emit('analysis_instructions', {
        'filepath': str(filepath),
        'instructions': [
            'Data saved! Run Claude Code in terminal:',
            'claude dashboard/analyze_brainwaves.py'
        ]
    })

# GPIO Configuration endpoints
GPIO_CONFIG_FILE = Path(__file__).parent.parent / "gpio_config.json"

def load_gpio_config():
    """Load GPIO configuration from file"""
    if GPIO_CONFIG_FILE.exists():
        try:
            with open(GPIO_CONFIG_FILE, 'r') as f:
                return json.load(f)
        except:
            pass
    
    # Return defaults
    return {
        'cs_pin': 19,
        'button_pin_1': 26,
        'button_pin_2': 13,
        'gpio_chip': '0'
    }

def save_gpio_config(config):
    """Save GPIO configuration to file and update all scripts"""
    try:
        # Save to config file
        with open(GPIO_CONFIG_FILE, 'w') as f:
            json.dump(config, f, indent=2)
        
        # Update all PiEEG scripts
        scripts_to_update = [
            'GUI/2.Graph_Gpio_D_1_5_4.py',
            'GUI/2.Graph_Gpio_D_1_5_4_MQTT.py',
            'GUI/2.Graph_Gpio_D_1_5_4_MQTT_Auto.py',
            'GUI/1.Graph_Gpio_D _1_6_3.py',
            'GUI/2.Graph_Gpio_D _1_5_4_OS.py',
            'GUI/Graph_Gpio_D _1_5_4_not_spike.py'
        ]
        
        gui_path = Path(__file__).parent.parent
        
        for script_name in scripts_to_update:
            script_path = gui_path / script_name.replace('GUI/', '')
            if script_path.exists():
                update_script_gpio_config(script_path, config)
        
        return True
    except Exception as e:
        print(f"Error saving GPIO config: {e}")
        return False

def update_script_gpio_config(script_path, config):
    """Update GPIO configuration in a specific script"""
    try:
        with open(script_path, 'r') as f:
            content = f.read()
        
        # Update pin assignments
        content = re.sub(r'button_pin_1\s*=\s*\d+', f'button_pin_1 = {config["button_pin_1"]}', content)
        content = re.sub(r'button_pin_2\s*=\s*\d+', f'button_pin_2 = {config["button_pin_2"]}', content)
        content = re.sub(r'cs_pin\s*=\s*\d+', f'cs_pin = {config["cs_pin"]}', content)
        
        # Update GPIO chip
        if config['gpio_chip'] == '/dev/gpiochip4':
            content = re.sub(r'chip\s*=\s*gpiod\.chip\(["\']0["\']\)', 
                           'chip = gpiod.chip("/dev/gpiochip4")', content)
        else:
            content = re.sub(r'chip\s*=\s*gpiod\.chip\(["\'][^"\']+["\']\)', 
                           'chip = gpiod.chip("0")', content)
        
        with open(script_path, 'w') as f:
            f.write(content)
            
        print(f"Updated GPIO config in {script_path.name}")
    except Exception as e:
        print(f"Error updating {script_path.name}: {e}")

@app.route('/api/gpio-config', methods=['GET', 'POST'])
def api_gpio_config():
    """Get or set GPIO configuration"""
    if request.method == 'GET':
        return jsonify(load_gpio_config())
    
    elif request.method == 'POST':
        try:
            config = request.json
            
            # Validate configuration
            required_keys = ['cs_pin', 'button_pin_1', 'button_pin_2', 'gpio_chip']
            for key in required_keys:
                if key not in config:
                    return jsonify({'status': 'error', 'message': f'Missing {key}'}), 400
            
            # Check for valid pin numbers
            valid_pins = [5, 6, 12, 13, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27]
            for pin_key in ['cs_pin', 'button_pin_1', 'button_pin_2']:
                if config[pin_key] not in valid_pins:
                    return jsonify({'status': 'error', 'message': f'Invalid pin {config[pin_key]}'}), 400
            
            # Save configuration
            if save_gpio_config(config):
                return jsonify({'status': 'success', 'message': 'Configuration saved'})
            else:
                return jsonify({'status': 'error', 'message': 'Failed to save configuration'}), 500
                
        except Exception as e:
            return jsonify({'status': 'error', 'message': str(e)}), 500

def start_mqtt_client():
    """Start MQTT client in background"""
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_forever()
    except Exception as e:
        print(f"MQTT connection error: {e}")

if __name__ == '__main__':
    # Start MQTT client in background thread
    mqtt_thread = threading.Thread(target=start_mqtt_client, daemon=True)
    mqtt_thread.start()
    
    print("🧠 PiEEG Brainwave Dashboard Starting (Claude Code Edition)...")
    print("📊 Dashboard available at: http://localhost:5001")
    print("🤖 AI Analysis powered by Claude Code (no API key needed!)")
    print("📡 MQTT Topic:", MQTT_TOPIC)
    print("\n📁 Brainwave data will be saved to:", DATA_DIR)
    print("💡 Use 'claude dashboard/analyze_brainwaves.py' for AI analysis")
    
    # Run the app
    socketio.run(app, debug=False, host='0.0.0.0', port=5001, allow_unsafe_werkzeug=True)