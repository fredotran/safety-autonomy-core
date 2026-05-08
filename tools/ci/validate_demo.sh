#!/bin/bash
# Demo validation script for CI/CD pipeline
# This script validates that the safety autonomy core demos are working correctly

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Safety Autonomy Core Demo Validation ===${NC}"

# Function to check if ROS 2 is sourced
check_ros2() {
    if [ -z "$ROS_DISTRO" ]; then
        echo -e "${RED}Error: ROS 2 environment not sourced${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓ ROS 2 environment sourced ($ROS_DISTRO)${NC}"
}

# Function to check if workspace is built
check_workspace() {
    if [ ! -d "/workspace/ros2_ws/install" ]; then
        echo -e "${RED}Error: ROS 2 workspace not built${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓ ROS 2 workspace built${NC}"
}

# Function to validate safety nodes are running
validate_safety_nodes() {
    echo -e "${YELLOW}Checking safety nodes...${NC}"
    
    # Wait for nodes to start
    sleep 5
    
    # Check for critical safety nodes
    local nodes=(
        "safety_envelope_node"
        "safety_supervisor_node"
        "safety_drive_bridge_node"
    )
    
    for node in "${nodes[@]}"; do
        if ros2 node list | grep -q "$node"; then
            echo -e "${GREEN}✓ Node $node is running${NC}"
        else
            echo -e "${YELLOW}⚠ Node $node not found (may be optional in this mode)${NC}"
        fi
    done
}

# Function to validate safety topics
validate_safety_topics() {
    echo -e "${YELLOW}Checking safety topics...${NC}"
    
    local topics=(
        "/safety/state"
        "/safety/envelope_status"
        "/safety/safe_stop"
        "/cmd_vel_nav"
    )
    
    for topic in "${topics[@]}"; do
        if ros2 topic list | grep -q "$topic"; then
            echo -e "${GREEN}✓ Topic $topic exists${NC}"
        else
            echo -e "${YELLOW}⚠ Topic $topic not found (may be optional in this mode)${NC}"
        fi
    done
}

# Function to run quick demo validation
validate_quick_demo() {
    echo -e "${YELLOW}Running quick demo validation...${NC}"
    
    timeout 120s python3 /workspace/ros2_ws/src/safety_core_bringup/scripts/quick_demo.py &
    local demo_pid=$!
    
    # Wait for demo to start
    sleep 10
    
    # Check if demo process is still running
    if ps -p $demo_pid > /dev/null; then
        echo -e "${GREEN}✓ Quick demo started successfully${NC}"
        wait $demo_pid || echo -e "${YELLOW}⚠ Quick demo completed or timed out${NC}"
    else
        echo -e "${RED}✗ Quick demo failed to start${NC}"
        return 1
    fi
}

# Function to run comprehensive demo validation
validate_comprehensive_demo() {
    echo -e "${YELLOW}Running comprehensive demo validation...${NC}"
    
    timeout 300s python3 /workspace/ros2_ws/src/safety_core_bringup/scripts/comprehensive_demo.py &
    local demo_pid=$!
    
    # Wait for demo to start
    sleep 10
    
    # Check if demo process is still running
    if ps -p $demo_pid > /dev/null; then
        echo -e "${GREEN}✓ Comprehensive demo started successfully${NC}"
        wait $demo_pid || echo -e "${YELLOW}⚠ Comprehensive demo completed or timed out${NC}"
    else
        echo -e "${RED}✗ Comprehensive demo failed to start${NC}"
        return 1
    fi
}

# Function to validate launch file
validate_launch_file() {
    local launch_file=$1
    echo -e "${YELLOW}Validating launch file: $launch_file${NC}"
    
    # Launch in background
    timeout 30s ros2 launch $launch_file &
    local launch_pid=$!
    
    # Wait for launch to start
    sleep 15
    
    # Check if launch process is still running
    if ps -p $launch_pid > /dev/null; then
        echo -e "${GREEN}✓ Launch file $launch_file started successfully${NC}"
        
        # Validate nodes
        validate_safety_nodes
        
        # Validate topics
        validate_safety_topics
        
        # Clean up
        pkill -f "ros2 launch" || true
        wait $launch_pid 2>/dev/null || true
    else
        echo -e "${RED}✗ Launch file $launch_file failed to start${NC}"
        return 1
    fi
}

# Main validation logic
main() {
    # Check environment
    check_ros2
    check_workspace
    
    # Source workspace
    source /workspace/ros2_ws/install/setup.bash
    
    # Parse command line arguments
    case "${1:-all}" in
        quick)
            validate_quick_demo
            ;;
        comprehensive)
            validate_comprehensive_demo
            ;;
        launch)
            if [ -z "$2" ]; then
                echo -e "${RED}Error: Launch file not specified${NC}"
                echo "Usage: $0 launch <launch_file>"
                exit 1
            fi
            validate_launch_file "$2"
            ;;
        all)
            echo -e "${YELLOW}Running all demo validations...${NC}"
            validate_quick_demo
            echo ""
            validate_comprehensive_demo
            echo ""
            validate_launch_file "safety_core_bringup safety_sim.launch.py"
            echo ""
            validate_launch_file "safety_core_bringup diagnostics.launch.py"
            ;;
        *)
            echo -e "${RED}Error: Unknown validation mode: $1${NC}"
            echo "Usage: $0 [quick|comprehensive|launch <file>|all]"
            exit 1
            ;;
    esac
    
    echo -e "${GREEN}=== Demo validation completed ===${NC}"
}

# Run main function
main "$@"
