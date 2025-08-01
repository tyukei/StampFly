#!/usr/bin/env python3
"""
PiEEG Brainwave Dashboard with Gemini AI Integration
A dynamic web dashboard for real-time brainwave visualization and AI analysis
"""

import asyncio
import json
import time
import threading
from datetime import datetime
from flask import Flask, render_template, jsonify, request
from flask_socketio import SocketIO, emit
import paho.mqtt.client as mqtt
import anthropic
from collections import deque
import numpy as np
import os
from typing import Dict, List, Optional

app = Flask(__name__)
app.config['SECRET_KEY'] = 'pieeg_dashboard_secret'
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# Global data storage
brainwave_data = deque(maxlen=1000)  # Store last 1000 readings
current_state = {
    'theta_power': 0,
    'alpha_power': 0,
    'beta_power': 0,
    'gamma_power': 0,
    'dominant_wave': 'alpha',
    'timestamp': time.time()
}

# AI Analysis storage
ai_insights = {
    'current_analysis': '',
    'recommendations': [],
    'mood_assessment': '',
    'stress_level': 0,
    'focus_level': 0,
    'last_updated': time.time()
}

# MQTT Configuration
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "pieeg/m5stamp/commands"

# Claude AI Configuration
CLAUDE_API_KEY = os.getenv('ANTHROPIC_API_KEY', 'your-claude-api-key-here')
if CLAUDE_API_KEY != 'your-claude-api-key-here':
    claude_client = anthropic.Anthropic(api_key=CLAUDE_API_KEY)
else:
    claude_client = None

class BrainwaveAnalyzer:
    def __init__(self):
        self.history = deque(maxlen=100)
        
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

analyzer = BrainwaveAnalyzer()

# MQTT Setup
mqtt_client = None

def on_mqtt_connect(client, userdata, flags, rc):
    """MQTT connection callback"""
    if rc == 0:
        print(f"✅ Connected to MQTT broker: {MQTT_BROKER}")
        client.subscribe(MQTT_TOPIC)
        print(f"📡 Subscribed to topic: {MQTT_TOPIC}")
    else:
        print(f"❌ MQTT connection failed with code: {rc}")

def on_mqtt_message(client, userdata, msg):
    """MQTT message callback"""
    global current_state
    try:
        data = json.loads(msg.payload.decode())
        
        # Update current state
        current_state.update({
            'theta_power': data.get('theta_power', 0),
            'alpha_power': data.get('alpha_power', 0),
            'beta_power': data.get('beta_power', 0),
            'gamma_power': data.get('gamma_power', 0),
            'dominant_wave': data.get('dominant_wave', 'unknown'),
            'timestamp': data.get('timestamp', time.time())
        })
        
        # Store in history
        brainwave_data.append(current_state.copy())
        
        # Emit to connected clients
        socketio.emit('brainwave_update', current_state)
        
        # Analyze patterns
        analysis = analyzer.analyze_patterns(current_state)
        socketio.emit('pattern_analysis', analysis)
        
    except Exception as e:
        print(f"❌ Error processing MQTT message: {e}")

# Initialize MQTT client
mqtt_client = mqtt.Client(client_id="pieeg-dashboard", protocol=mqtt.MQTTv311)
mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message

def get_claude_analysis(brainwave_data: List[Dict]) -> Dict:
    """Get AI analysis from Claude"""
    if not claude_client:
        return {
            'analysis': 'Claude API key not configured. Please set ANTHROPIC_API_KEY environment variable.',
            'recommendations': ['Set up Claude API key for AI insights'],
            'mood_assessment': 'Unknown'
        }
    
    try:
        # Prepare data for Claude
        if not brainwave_data:
            return {'analysis': 'No data available', 'recommendations': [], 'mood_assessment': 'Unknown'}
        
        latest_data = brainwave_data[-1] if brainwave_data else {}
        
        # Get recent trend if we have enough data
        if len(brainwave_data) >= 5:
            recent_avg = {
                'theta': np.mean([d.get('theta_power', 0) for d in brainwave_data[-5:]]),
                'alpha': np.mean([d.get('alpha_power', 0) for d in brainwave_data[-5:]]),
                'beta': np.mean([d.get('beta_power', 0) for d in brainwave_data[-5:]]),
                'gamma': np.mean([d.get('gamma_power', 0) for d in brainwave_data[-5:]])
            }
            trend_info = f"""
            Recent 5-reading averages:
            - Theta: {recent_avg['theta']:.6f}
            - Alpha: {recent_avg['alpha']:.6f}
            - Beta: {recent_avg['beta']:.6f}
            - Gamma: {recent_avg['gamma']:.6f}
            """
        else:
            trend_info = "Not enough data for trend analysis."
        
        prompt = f"""You are an expert neuroscientist analyzing EEG brainwave data. Please provide a concise analysis.

Current brainwave readings:
- Theta (4-8 Hz): {latest_data.get('theta_power', 0):.6f} - Deep relaxation, meditation, creativity
- Alpha (8-12 Hz): {latest_data.get('alpha_power', 0):.6f} - Calm alertness, relaxed focus
- Beta (13-30 Hz): {latest_data.get('beta_power', 0):.6f} - Active concentration, analytical thinking
- Gamma (30-100 Hz): {latest_data.get('gamma_power', 0):.6f} - High-level cognitive processing
- Dominant wave: {latest_data.get('dominant_wave', 'unknown')}

{trend_info}

Please provide your analysis in this exact format:
ANALYSIS: [2-3 sentences about current mental state]
RECOMMENDATIONS: [3 bullet points with specific actionable advice]
MOOD: [Single phrase mood assessment]

Keep responses practical and evidence-based."""
        
        response = claude_client.messages.create(
            model="claude-3-haiku-20240307",
            max_tokens=500,
            messages=[{
                "role": "user",
                "content": prompt
            }]
        )
        
        analysis_text = response.content[0].text
        
        # Parse Claude's structured response
        analysis_lines = analysis_text.split('\n')
        
        analysis = ""
        recommendations = []
        mood_assessment = "Unknown"
        
        for line in analysis_lines:
            if line.startswith('ANALYSIS:'):
                analysis = line.replace('ANALYSIS:', '').strip()
            elif line.startswith('RECOMMENDATIONS:'):
                continue
            elif line.startswith('MOOD:'):
                mood_assessment = line.replace('MOOD:', '').strip()
            elif line.startswith('- ') or line.startswith('• '):
                recommendations.append(line.strip('- •').strip())
        
        # Fallback if parsing fails
        if not analysis:
            analysis = analysis_text
        if not recommendations:
            recommendations = [
                'Monitor your brainwave patterns for trends',
                'Consider mindfulness practices for balance',
                'Maintain regular sleep and exercise routines'
            ]
        
        return {
            'analysis': analysis,
            'recommendations': recommendations,
            'mood_assessment': mood_assessment
        }
        
    except Exception as e:
        return {
            'analysis': f'Claude analysis unavailable: {str(e)}',
            'recommendations': ['Check your internet connection', 'Verify Claude API key'],
            'mood_assessment': 'Unknown'
        }

def on_mqtt_connect(client, userdata, flags, rc):
    """MQTT connection callback"""
    print(f"Connected to MQTT broker with result code {rc}")
    client.subscribe(MQTT_TOPIC)
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
        
        # Analyze patterns
        analysis = analyzer.analyze_patterns(data)
        
        # Emit real-time data to clients
        socketio.emit('brainwave_data', {
            'current': current_state,
            'analysis': analysis,
            'timestamp': datetime.now().isoformat()
        })
        
        # Trigger AI analysis every 10 readings
        if len(brainwave_data) % 10 == 0:
            threading.Thread(target=update_ai_analysis, daemon=True).start()
        
    except Exception as e:
        print(f"Error processing MQTT message: {e}")

def update_ai_analysis():
    """Update AI analysis in background"""
    global ai_insights
    
    try:
        recent_data = list(brainwave_data)[-20:] if len(brainwave_data) >= 20 else list(brainwave_data)
        ai_result = get_claude_analysis(recent_data)
        
        ai_insights.update(ai_result)
        ai_insights['last_updated'] = time.time()
        
        # Emit to clients
        socketio.emit('ai_analysis', ai_insights)
        
    except Exception as e:
        print(f"Error updating AI analysis: {e}")

# Initialize MQTT client
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
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
    return jsonify(ai_insights)

@app.route('/api/trigger-analysis', methods=['POST'])
def api_trigger_analysis():
    """Manually trigger AI analysis"""
    threading.Thread(target=update_ai_analysis, daemon=True).start()
    return jsonify({'status': 'triggered'})

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
    threading.Thread(target=update_ai_analysis, daemon=True).start()

def start_mqtt_client():
    """Start MQTT client in background"""
    try:
        print(f"🔄 Connecting to MQTT broker {MQTT_BROKER}...")
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_forever()
    except Exception as e:
        print(f"MQTT connection error: {e}")

# Initialize MQTT client when module is imported
print(f"📡 Initializing MQTT client for topic: {MQTT_TOPIC}")
mqtt_thread = threading.Thread(target=start_mqtt_client, daemon=True)
mqtt_thread.start()

if __name__ == '__main__':
    print("🧠 PiEEG Brainwave Dashboard Starting...")
    print("📊 Dashboard available at: http://localhost:5000")
    print("🤖 AI Analysis powered by Gemini")
    print("📡 MQTT Topic:", MQTT_TOPIC)
    
    # Run the app
    socketio.run(app, debug=True, host='0.0.0.0', port=5000)