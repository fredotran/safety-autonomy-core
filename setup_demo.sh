#!/bin/bash
# Setup and launch script for safety-autonomy-core ROS 2 AGV warehouse demo
# This script builds the workspace and launches the simulation
# Supports both local ROS2 installation and Docker containerization

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Safety Autonomy Core ROS 2 Demo Setup ===${NC}"
echo -e "${BLUE}Default mode: Docker (containerized, no local ROS2 required)${NC}"
echo -e "${YELLOW}Use --local flag for local ROS2 installation${NC}"
echo ""

# Check if we're in the repository root
if [ ! -d "ros2" ]; then
    echo -e "${RED}Error: Please run this script from the repository root${NC}"
    exit 1
fi

# Check if local mode is requested
USE_LOCAL=false
if [ "$1" = "--local" ] || [ "$1" = "-l" ]; then
    USE_LOCAL=true
    shift
fi

# Docker is the default, local requires explicit flag
USE_DOCKER=true
if [ "$USE_LOCAL" = true ]; then
    USE_DOCKER=false
fi

if [ "$USE_DOCKER" = true ]; then
    echo -e "${GREEN}Using Docker deployment mode${NC}"
else
    echo -e "${GREEN}Using local ROS2 deployment mode${NC}"
fi

# Docker mode logic
if [ "$USE_DOCKER" = true ]; then
    # Check if Docker is installed
    if ! command -v docker &> /dev/null; then
        echo -e "${RED}Error: Docker is not installed. Please install Docker first.${NC}"
        echo "  Visit https://docs.docker.com/get-docker/ for installation instructions."
        exit 1
    fi
    
    # Check if Docker Compose is installed
    if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
        echo -e "${RED}Error: Docker Compose is not installed. Please install Docker Compose first.${NC}"
        exit 1
    fi
    
    # Check if X11 forwarding is set up
    if [ -z "$DISPLAY" ]; then
        echo -e "${YELLOW}Warning: DISPLAY environment variable not set. GUI applications may not work.${NC}"
        echo "  If you want to run Gazebo/RViz with display, ensure X11 forwarding is configured."
    fi
    
    # Set up X11 authorization for display forwarding
    if [ -n "$DISPLAY" ]; then
        xhost +local:docker > /dev/null 2>&1 || true
    fi
    
    echo -e "${GREEN}Building Docker image with workspace...${NC}"
    
    # Use docker compose if available, otherwise docker-compose
    if docker compose version &> /dev/null; then
        DOCKER_COMPOSE="docker compose"
    else
        DOCKER_COMPOSE="docker-compose"
    fi
    
    # Build the Docker image (includes workspace build)
    $DOCKER_COMPOSE build
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}Docker build failed. Please check the errors above.${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Docker image with workspace built successfully.${NC}"
    echo ""
    echo -e "${BLUE}=== Docker Launch Options ===${NC}"
    echo "1. Full demo (Gazebo + Safety Stack + Nav2 + RViz) - Wheel Odometry Navigation"
    echo "2. Safety stack + Simulation (Gazebo + Safety Nodes + RViz)"
    echo "3. Safety stack only (no simulation, for physical robot)"
    echo "4. Simulation only (Gazebo only)"
    echo "5. Interactive bash shell in container"
    echo ""
    read -p "Select option [1-5]: " choice
    
    # Launch the appropriate demo in Docker
    case $choice in
        1)
            echo -e "${GREEN}Launching full demo in Docker...${NC}"
            $DOCKER_COMPOSE run --rm safety-autonomy-demo \
                bash -c "source install/setup.bash && ros2 launch safety_core_bringup agv_warehouse.launch.py"
            ;;
        2)
            echo -e "${GREEN}Launching safety stack with simulation in Docker...${NC}"
            $DOCKER_COMPOSE run --rm safety-autonomy-demo \
                bash -c "source install/setup.bash && ros2 launch safety_core_bringup safety_sim.launch.py"
            ;;
        3)
            echo -e "${GREEN}Launching safety stack only in Docker...${NC}"
            $DOCKER_COMPOSE run --rm safety-autonomy-demo \
                bash -c "source install/setup.bash && ros2 launch safety_core_bringup safety_only.launch.py"
            ;;
        4)
            echo -e "${GREEN}Launching simulation only in Docker...${NC}"
            $DOCKER_COMPOSE run --rm safety-autonomy-demo \
                bash -c "source install/setup.bash && ros2 launch safety_core_sim sim_only.launch.py"
            ;;
        5)
            echo -e "${GREEN}Launching interactive bash shell in Docker...${NC}"
            $DOCKER_COMPOSE run --rm safety-autonomy-demo
            ;;
        *)
            echo -e "${RED}Invalid option. Defaulting to safety stack with simulation...${NC}"
            $DOCKER_COMPOSE run --rm safety-autonomy-demo \
                bash -c "source install/setup.bash && ros2 launch safety_core_bringup safety_sim.launch.py"
            ;;
    esac
    
    # Clean up X11 authorization
    if [ -n "$DISPLAY" ]; then
        xhost -local:docker > /dev/null 2>&1 || true
    fi
    
    exit 0
fi

# Local ROS2 mode logic
# Source ROS 2 environment
if [ -f "/opt/ros/jazzy/setup.bash" ]; then
    echo -e "${GREEN}Sourcing ROS 2 Jazzy environment...${NC}"
    source /opt/ros/jazzy/setup.bash
elif [ -f "/opt/ros/humble/setup.bash" ]; then
    echo -e "${YELLOW}Warning: ROS 2 Humble detected (Jazzy recommended)${NC}"
    source /opt/ros/humble/setup.bash
else
    echo -e "${RED}Error: ROS 2 not found. Please install ROS 2 Jazzy or Humble.${NC}"
    echo -e "${YELLOW}Alternatively, use the default Docker mode (no local ROS2 required).${NC}"
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
echo "1. Full demo (Gazebo + Safety Stack + Nav2 + RViz) - Wheel Odometry Navigation"
echo "2. Safety stack + Simulation (Gazebo + Safety Nodes + RViz)"
echo "3. Safety stack only (no simulation, for physical robot)"
echo "4. Simulation only (Gazebo only)"
echo ""
read -p "Select option [1-4]: " choice

case $choice in
    1)
        echo -e "${GREEN}Launching full demo...${NC}"
        ros2 launch safety_core_bringup agv_warehouse.launch.py
        ;;
    2)
        echo -e "${GREEN}Launching safety stack with simulation...${NC}"
        ros2 launch safety_core_bringup safety_sim.launch.py
        ;;
    3)
        echo -e "${GREEN}Launching safety stack only...${NC}"
        ros2 launch safety_core_bringup safety_only.launch.py
        ;;
    4)
        echo -e "${GREEN}Launching simulation only...${NC}"
        ros2 launch safety_core_sim sim_only.launch.py
        ;;
    *)
        echo -e "${RED}Invalid option. Defaulting to safety stack with simulation...${NC}"
        ros2 launch safety_core_bringup safety_sim.launch.py
        ;;
esac
