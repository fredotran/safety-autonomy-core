#!/bin/bash
# Setup and launch script for safety-autonomy-core ROS 2 AGV warehouse demo
# This script builds the workspace and launches the simulation

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Safety Autonomy Core ROS 2 Demo Setup ===${NC}"

# Check if we're in the repository root
if [ ! -d "ros2" ]; then
    echo -e "${RED}Error: Please run this script from the repository root${NC}"
    exit 1
fi

# Source ROS 2 environment
if [ -f "/opt/ros/jazzy/setup.bash" ]; then
    echo -e "${GREEN}Sourcing ROS 2 Jazzy environment...${NC}"
    source /opt/ros/jazzy/setup.bash
elif [ -f "/opt/ros/humble/setup.bash" ]; then
    echo -e "${YELLOW}Warning: ROS 2 Humble detected (Jazzy recommended)${NC}"
    source /opt/ros/humble/setup.bash
else
    echo -e "${RED}Error: ROS 2 not found. Please install ROS 2 Jazzy or Humble.${NC}"
    exit 1
fi

# Check if Nav2 is installed (optional)
if ! dpkg -l | grep -q ros-jazzy-navigation2; then
    echo -e "${YELLOW}Note: Nav2 not installed. Install with:${NC}"
    echo "  sudo apt install ros-jazzy-navigation2 ros-jazzy-nav2-bringup ros-jazzy-behaviortree-cpp-v3"
    echo -e "${YELLOW}Demo will launch without Nav2 (safety stack only).${NC}"
fi

# Build the workspace
echo -e "${GREEN}Building workspace...${NC}"
cd ros2
colcon build \
    --cmake-args -DSAFETY_CORE_ENABLE_AMENT=ON \
                 -DSAFETY_CORE_ENABLE_SANITIZERS=OFF \
                 -DSAFETY_CORE_ENABLE_WERROR=OFF

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed. Please check the errors above.${NC}"
    exit 1
fi

# Source the workspace
echo -e "${GREEN}Sourcing workspace...${NC}"
source install/setup.bash

# Launch options
echo ""
echo -e "${GREEN}=== Launch Options ===${NC}"
echo "1. Full demo (Gazebo + Safety Stack + Nav2 + SLAM + RViz)"
echo "2. Safety stack only (Gazebo + Safety Nodes)"
echo "3. Simulation only (Gazebo only)"
echo ""
read -p "Select option [1-3]: " choice

case $choice in
    1)
        echo -e "${GREEN}Launching full demo...${NC}"
        ros2 launch safety_core_bringup agv_warehouse.launch.py
        ;;
    2)
        echo -e "${GREEN}Launching safety stack only...${NC}"
        ros2 launch safety_core_bringup safety_only.launch.py
        ;;
    3)
        echo -e "${GREEN}Launching simulation only...${NC}"
        ros2 launch safety_core_sim sim_only.launch.py
        ;;
    *)
        echo -e "${RED}Invalid option. Defaulting to safety stack only...${NC}"
        ros2 launch safety_core_bringup safety_only.launch.py
        ;;
esac
