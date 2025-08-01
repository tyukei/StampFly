#!/bin/bash
# start.sh - Launch PiEEG-16 EEG collection and dashboard

# Set script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}[PiEEG-16] Starting System...${NC}"

# Check if virtual environment exists
if [ ! -f "venv/bin/activate" ] && [ ! -f ".venv/bin/activate" ] && [ ! -f "GUI/venv/bin/activate" ] && [ ! -f "GUI/.venv/bin/activate" ]; then
    echo -e "${YELLOW}No virtual environment found. Creating one...${NC}"
    echo -e "${BLUE}Setting up Python virtual environment...${NC}"
    python -m venv venv
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[✓] Virtual environment created${NC}"
        source venv/bin/activate
        echo -e "${BLUE}Installing dependencies...${NC}"
        if [ -f "requirements.txt" ]; then
            pip install -r requirements.txt
        elif [ -f "GUI/requirements.txt" ]; then
            pip install -r GUI/requirements.txt
        else
            echo -e "${YELLOW}No requirements.txt found. Please install dependencies manually.${NC}"
        fi
    else
        echo -e "${RED}Failed to create virtual environment. Please check Python installation.${NC}"
        exit 1
    fi
fi

# Function to run Python commands with proper environment
run_python() {
    local script=$1
    shift
    
    # Check for venv in common locations (preferred)
    if [ -f "venv/bin/activate" ]; then
        echo -e "${GREEN}Using venv...${NC}"
        source venv/bin/activate
        python "$script" "$@"
    elif [ -f ".venv/bin/activate" ]; then
        echo -e "${GREEN}Using .venv...${NC}"
        source .venv/bin/activate
        python "$script" "$@"
    elif [ -f "GUI/venv/bin/activate" ]; then
        echo -e "${GREEN}Using GUI/venv...${NC}"
        source GUI/venv/bin/activate
        python "$script" "$@"
    elif [ -f "GUI/.venv/bin/activate" ]; then
        echo -e "${GREEN}Using GUI/.venv...${NC}"
        source GUI/.venv/bin/activate
        python "$script" "$@"
    # Check for activated virtual environment
    elif [ -n "$VIRTUAL_ENV" ]; then
        echo -e "${GREEN}Using activated virtual environment: $VIRTUAL_ENV${NC}"
        python "$script" "$@"
    # Check for uv as fallback
    elif command -v uv &> /dev/null; then
        echo -e "${YELLOW}Using uv to run: $script${NC}"
        uv run python "$script" "$@"
    else
        echo -e "${RED}Error: No virtual environment found${NC}"
        echo -e "${YELLOW}Please create a virtual environment first:${NC}"
        echo -e "${YELLOW}  python -m venv venv${NC}"
        echo -e "${YELLOW}  source venv/bin/activate${NC}"
        echo -e "${YELLOW}  pip install -r requirements.txt${NC}"
        exit 1
    fi
}

# Check if running on Raspberry Pi
if [ ! -f /sys/firmware/devicetree/base/model ]; then
    echo -e "${RED}Warning: Not running on Raspberry Pi. GPIO access may fail.${NC}"
fi

# Function to cleanup on exit
cleanup() {
    echo -e "\n${BLUE}Shutting down PiEEG-16 system...${NC}"
    # Kill all background processes
    kill $(jobs -p) 2>/dev/null
    # Run GPIO cleanup
    cd "$SCRIPT_DIR/GUI" && run_python cleanup_gpio.py 2>/dev/null
    echo -e "${GREEN}[✓] Cleanup complete${NC}"
    exit 0
}

# Set trap for cleanup on script exit
trap cleanup EXIT INT TERM

# Check dependencies
echo -e "${BLUE}Checking dependencies...${NC}"
cd GUI

# Check if we can run Python
if ! run_python -c "print('Python OK')" &>/dev/null; then
    echo -e "${RED}Error: Python not accessible. Please check your environment.${NC}"
    exit 1
fi

# Start EEG data collection with MQTT in background
echo -e "${BLUE}Starting EEG data collection and MQTT publisher...${NC}"
run_python 2.Graph_Gpio_D_1_5_4.py &
EEG_PID=$!
sleep 3  # Wait for initialization

# Check if EEG process is still running
if ! kill -0 $EEG_PID 2>/dev/null; then
    echo -e "${RED}[✗] Failed to start EEG data collection${NC}"
    echo -e "${RED}Check that:"
    echo -e "  - You're running on a Raspberry Pi"
    echo -e "  - PiEEG-16 hardware is connected"
    echo -e "  - Required Python packages are installed${NC}"
    exit 1
fi
echo -e "${GREEN}[✓] EEG data collection started (PID: $EEG_PID)${NC}"

# Wait a bit more to ensure everything is initialized
sleep 2

# Ask user which dashboard version to use
echo -e "\n${BLUE}Select dashboard version:${NC}"
echo "1) Standard dashboard (requires Anthropic API key)"
echo "2) Claude Code dashboard (no API key needed)"
echo -e "\n${YELLOW}Waiting for your input...${NC}"
read -p "Enter choice (1 or 2): " choice

case $choice in
    1)
        # Check for API key
        if [ -z "$ANTHROPIC_API_KEY" ]; then
            echo -e "${RED}Error: ANTHROPIC_API_KEY not set${NC}"
            echo "Please set it with: export ANTHROPIC_API_KEY='your-key-here'"
            exit 1
        fi
        echo -e "${BLUE}Starting standard dashboard...${NC}"
        run_python dashboard/start_dashboard.py &
        DASHBOARD_URL="http://localhost:5000"
        ;;
    2)
        echo -e "${BLUE}Starting Claude Code dashboard...${NC}"
        run_python dashboard/start_dashboard_claude_code.py &
        DASHBOARD_URL="http://localhost:5001"
        ;;
    *)
        echo -e "${RED}Invalid choice${NC}"
        exit 1
        ;;
esac

DASHBOARD_PID=$!
sleep 3  # Wait for Flask to start

# Check if dashboard is running
if ! kill -0 $DASHBOARD_PID 2>/dev/null; then
    echo -e "${RED}[✗] Failed to start dashboard${NC}"
    echo -e "${RED}Check the console output above for errors${NC}"
    exit 1
fi
echo -e "${GREEN}[✓] Dashboard started (PID: $DASHBOARD_PID)${NC}"

# Optional: Start MQTT monitor in new terminal (if available)
if command -v gnome-terminal &> /dev/null; then
    echo -e "${BLUE}Starting MQTT monitor in new terminal...${NC}"
    gnome-terminal -- bash -c "cd $SCRIPT_DIR/GUI && $0 run_python m5stamp_mqtt_test.py; read -p 'Press enter to close...'"
elif command -v xterm &> /dev/null; then
    xterm -e "cd $SCRIPT_DIR/GUI && bash -c 'source $0; run_python m5stamp_mqtt_test.py; read -p \"Press enter to close...\"'" &
fi

# Display status
echo -e "\n${GREEN}================================================================${NC}"
echo -e "${GREEN}[✓] PiEEG-16 System Running${NC}"
echo -e "${GREEN}================================================================${NC}"
echo -e "[Dashboard] URL: ${BLUE}$DASHBOARD_URL${NC}"
echo -e "[MQTT] Topic: ${BLUE}pieeg/m5stamp/commands${NC}"
echo -e "[EEG] Collection: ${GREEN}Active${NC}"
echo -e "\nPress ${RED}Ctrl+C${NC} to stop all services"
echo -e "${GREEN}================================================================${NC}"

# Optional: Open browser
if command -v xdg-open &> /dev/null; then
    sleep 2
    echo -e "${BLUE}Opening dashboard in browser...${NC}"
    xdg-open "$DASHBOARD_URL" 2>/dev/null
elif [ "$(uname)" == "Darwin" ]; then
    # macOS
    open "$DASHBOARD_URL" 2>/dev/null
fi

# Keep script running and wait for interrupt
wait