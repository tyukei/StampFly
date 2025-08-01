#!/usr/bin/env python3
"""
Claude Code Brainwave Analysis Script
This script analyzes saved brainwave data and generates AI insights
"""

import json
import sys
from pathlib import Path
from datetime import datetime
import numpy as np

# Data directory
DATA_DIR = Path(__file__).parent / "brainwave_data"

def load_latest_data():
    """Load the most recent brainwave data file"""
    data_file = DATA_DIR / "latest_brainwave_data.json"
    
    if not data_file.exists():
        print("❌ No brainwave data found! Please record some data first.")
        return None
    
    with open(data_file, 'r') as f:
        return json.load(f)

def analyze_brainwave_patterns(data):
    """Analyze brainwave patterns and generate insights"""
    
    if not data or 'brainwave_data' not in data:
        return None
    
    brainwave_data = data['brainwave_data']
    stats = data.get('statistics', {})
    
    # Get the dominant wave
    dominant_wave = stats.get('dominant_wave', 'unknown')
    
    # Calculate recent trends if we have enough data
    if len(brainwave_data) >= 10:
        recent_data = brainwave_data[-10:]
        
        theta_trend = np.mean([d.get('theta_power', 0) for d in recent_data])
        alpha_trend = np.mean([d.get('alpha_power', 0) for d in recent_data])
        beta_trend = np.mean([d.get('beta_power', 0) for d in recent_data])
        gamma_trend = np.mean([d.get('gamma_power', 0) for d in recent_data])
        
        # Determine mental state based on wave patterns
        mental_state = determine_mental_state(theta_trend, alpha_trend, beta_trend, gamma_trend, dominant_wave)
        
        # Generate recommendations
        recommendations = generate_recommendations(mental_state, dominant_wave, theta_trend, alpha_trend, beta_trend, gamma_trend)
        
        # Assess mood
        mood = assess_mood(theta_trend, alpha_trend, beta_trend, gamma_trend)
        
        # Calculate levels
        stress_level = min(100, int(beta_trend * 1000))
        focus_level = min(100, int((beta_trend + gamma_trend) * 500))
        relaxation_level = min(100, int((theta_trend + alpha_trend) * 500))
        
        return {
            'current_analysis': mental_state,
            'recommendations': recommendations,
            'mood_assessment': mood,
            'stress_level': stress_level,
            'focus_level': focus_level,
            'relaxation_level': relaxation_level,
            'dominant_wave': dominant_wave,
            'timestamp': datetime.now().isoformat()
        }
    
    return {
        'current_analysis': 'Not enough data for analysis. Please record more brainwave data.',
        'recommendations': ['Continue recording brainwave data', 'Ensure good electrode contact', 'Minimize movement during recording'],
        'mood_assessment': 'Unknown - insufficient data',
        'stress_level': 0,
        'focus_level': 0,
        'relaxation_level': 0
    }

def determine_mental_state(theta, alpha, beta, gamma, dominant_wave):
    """Determine mental state based on brainwave patterns"""
    
    states = []
    
    # Analyze based on dominant wave
    if dominant_wave == 'theta':
        if theta > 0.002:
            states.append("deep meditation or drowsiness")
        else:
            states.append("light relaxation")
    
    elif dominant_wave == 'alpha':
        if alpha > 0.003:
            states.append("calm and relaxed awareness")
        else:
            states.append("mild relaxation")
    
    elif dominant_wave == 'beta':
        if beta > 0.002:
            states.append("active concentration and analytical thinking")
        else:
            states.append("alert and engaged")
    
    elif dominant_wave == 'gamma':
        states.append("high-level cognitive processing and awareness")
    
    # Additional pattern analysis
    if theta > 0.002 and alpha > 0.002:
        states.append("creative and intuitive state")
    
    if beta > 0.003 and gamma > 0.001:
        states.append("intense focus and problem-solving")
    
    if alpha < 0.001 and beta > 0.002:
        states.append("possible stress or anxiety")
    
    # Combine states into analysis
    if states:
        analysis = f"Your brainwave patterns indicate {', '.join(states)}. "
        analysis += f"The dominant {dominant_wave} waves suggest "
        
        wave_meanings = {
            'theta': "a deeply relaxed or meditative state",
            'alpha': "a calm and mindful state",
            'beta': "an active and focused mental state",
            'gamma': "heightened awareness and cognitive processing"
        }
        
        analysis += wave_meanings.get(dominant_wave, "an active mental state") + "."
    else:
        analysis = "Your brainwave patterns show balanced activity across all frequency bands."
    
    return analysis

def generate_recommendations(mental_state, dominant_wave, theta, alpha, beta, gamma):
    """Generate personalized recommendations based on brainwave patterns"""
    
    recommendations = []
    
    # Stress management
    if beta > 0.003 or "stress" in mental_state.lower() or "anxiety" in mental_state.lower():
        recommendations.append("Practice deep breathing exercises to reduce stress levels")
        recommendations.append("Consider a 5-minute mindfulness meditation")
    
    # Focus enhancement
    if dominant_wave == 'alpha' and beta < 0.001:
        recommendations.append("Engage in light physical activity to increase alertness")
        recommendations.append("Try a mentally stimulating task to boost focus")
    
    # Relaxation
    if dominant_wave == 'beta' and alpha < 0.001:
        recommendations.append("Take regular breaks to prevent mental fatigue")
        recommendations.append("Listen to calming music or nature sounds")
    
    # Meditation optimization
    if dominant_wave == 'theta':
        recommendations.append("This is an excellent state for meditation - continue your practice")
        recommendations.append("Use this receptive state for visualization or creative thinking")
    
    # General wellness
    if dominant_wave == 'alpha':
        recommendations.append("Your brain is in an optimal state for learning and creativity")
        recommendations.append("Maintain this balanced state with regular mindfulness practices")
    
    # High cognitive load
    if gamma > 0.002:
        recommendations.append("Your brain is highly active - ensure adequate rest periods")
        recommendations.append("Stay hydrated and take breaks to maintain peak performance")
    
    # If no specific recommendations, provide general ones
    if not recommendations:
        recommendations = [
            "Maintain regular sleep schedule for optimal brain function",
            "Practice mindfulness meditation for 10 minutes daily",
            "Stay hydrated and take regular breaks during work"
        ]
    
    return recommendations[:3]  # Return top 3 recommendations

def assess_mood(theta, alpha, beta, gamma):
    """Assess mood based on brainwave patterns"""
    
    mood_indicators = []
    
    # High alpha typically indicates positive mood
    if alpha > 0.003:
        mood_indicators.append("relaxed")
        mood_indicators.append("content")
    
    # High beta might indicate stress or excitement
    if beta > 0.003:
        if alpha < 0.001:
            mood_indicators.append("stressed")
            mood_indicators.append("anxious")
        else:
            mood_indicators.append("energetic")
            mood_indicators.append("motivated")
    
    # High theta suggests deep relaxation or drowsiness
    if theta > 0.003:
        mood_indicators.append("peaceful")
        mood_indicators.append("contemplative")
    
    # Balanced waves
    if 0.001 < alpha < 0.003 and 0.001 < beta < 0.003:
        mood_indicators.append("balanced")
        mood_indicators.append("focused")
    
    # High gamma indicates heightened awareness
    if gamma > 0.002:
        mood_indicators.append("alert")
        mood_indicators.append("engaged")
    
    if mood_indicators:
        # Select the most relevant mood descriptors
        if "stressed" in mood_indicators or "anxious" in mood_indicators:
            return "Stressed but alert"
        elif "relaxed" in mood_indicators and "content" in mood_indicators:
            return "Calm and content"
        elif "energetic" in mood_indicators:
            return "Energetic and motivated"
        elif "peaceful" in mood_indicators:
            return "Deeply relaxed"
        else:
            return f"{mood_indicators[0].capitalize()} and {mood_indicators[1]}"
    
    return "Neutral and balanced"

def save_analysis(analysis):
    """Save the analysis results for the dashboard to read"""
    analysis_file = DATA_DIR / "claude_analysis.json"
    
    with open(analysis_file, 'w') as f:
        json.dump(analysis, f, indent=2)
    
    print(f"✅ Analysis saved to {analysis_file}")

def main():
    """Main analysis function"""
    print("🧠 Claude Code Brainwave Analysis")
    print("="*50)
    
    # Load data
    data = load_latest_data()
    if not data:
        return
    
    print(f"📊 Loaded {data.get('data_points', 0)} data points")
    print(f"⏱️  Duration: {data.get('duration_seconds', 0):.1f} seconds")
    
    # Analyze patterns
    print("\n🔍 Analyzing brainwave patterns...")
    analysis = analyze_brainwave_patterns(data)
    
    if analysis:
        # Display results
        print("\n📈 Analysis Results:")
        print("-"*50)
        print(f"\n🧘 Mental State:\n{analysis['current_analysis']}")
        
        print(f"\n😊 Mood Assessment: {analysis['mood_assessment']}")
        
        print(f"\n📊 Levels:")
        print(f"   • Stress Level: {analysis['stress_level']}%")
        print(f"   • Focus Level: {analysis['focus_level']}%")
        print(f"   • Relaxation Level: {analysis['relaxation_level']}%")
        
        print(f"\n💡 Recommendations:")
        for i, rec in enumerate(analysis['recommendations'], 1):
            print(f"   {i}. {rec}")
        
        # Save analysis
        save_analysis(analysis)
        
        print("\n✨ Analysis complete! The dashboard will automatically load these insights.")
        print("💡 Tip: Keep the dashboard open to see real-time updates!")
    else:
        print("❌ Analysis failed. Please check your data.")

if __name__ == "__main__":
    main()