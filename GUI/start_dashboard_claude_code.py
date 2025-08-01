#!/usr/bin/env python3
"""
Start the PiEEG Dashboard (Claude Code Edition)
No API key required - uses Claude Code for analysis!
"""

import subprocess
import sys
import os
from pathlib import Path

def check_dependencies():
    """Check if required packages are installed"""
    required_packages = ['flask', 'flask-socketio', 'paho-mqtt', 'numpy']
    missing = []
    
    for package in required_packages:
        try:
            __import__(package.replace('-', '_'))
        except ImportError:
            missing.append(package)
    
    if missing:
        print(f"❌ Missing packages: {', '.join(missing)}")
        print("Installing missing packages...")
        
        # Try to install with uv if available
        try:
            subprocess.run(['uv', 'sync'], cwd=Path(__file__).parent, check=True)
        except:
            print("Please install missing packages manually or run: uv sync")
            return False
    
    return True

def main():
    """Main launcher function"""
    print("🧠 PiEEG Dashboard Launcher (Claude Code Edition)")
    print("="*50)
    print("✨ No API key needed - uses Claude Code for AI analysis!")
    print()
    
    # Check dependencies
    if not check_dependencies():
        sys.exit(1)
    
    # Instructions
    print("📋 How to use:")
    print("1. This dashboard will start on http://localhost:5001")
    print("2. Connect your PiEEG device (run 2.Graph_Gpio_D_1_5_4.py)")
    print("3. Click 'Save for Analysis' in the dashboard")
    print("4. Run: claude dashboard/analyze_brainwaves.py")
    print("5. The analysis will appear in the dashboard automatically!")
    print()
    print("Starting dashboard...")
    print("-"*50)
    
    # Start the dashboard
    dashboard_path = Path(__file__).parent / "app_claude_code.py"
    
    try:
        subprocess.run([sys.executable, str(dashboard_path)])
    except KeyboardInterrupt:
        print("\n\n👋 Dashboard stopped. Goodbye!")
    except Exception as e:
        print(f"\n❌ Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()