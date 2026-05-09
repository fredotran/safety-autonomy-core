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

# Function to launch safety stack in background
launch_safety_stack() {
    echo -e "${YELLOW}Launching safety stack...${NC}"
    
    # Kill any existing safety processes
    pkill -f "safety_envelope_node" || true
    pkill -f "safety_supervisor_node" || true
    pkill -f "safety_drive_bridge_node" || true
    pkill -f "ros2 launch" || true
    sleep 2
    
    # Launch safety stack in background
    ros2 launch safety_core_bringup safety_sim.launch.py > /tmp/safety_stack.log 2>&1 &
    local launch_pid=$!
    
    # Wait for safety stack to initialize
    echo -e "${YELLOW}Waiting for safety stack to initialize...${NC}"
    sleep 10
    
    # Check if launch process is still running
    if ps -p $launch_pid > /dev/null; then
        echo -e "${GREEN}✓ Safety stack launched successfully (PID: $launch_pid)${NC}"
        echo $launch_pid > /tmp/safety_stack.pid
    else
        echo -e "${RED}✗ Safety stack failed to launch${NC}"
        echo -e "${RED}Log output:${NC}"
        cat /tmp/safety_stack.log
        return 1
    fi
}

# Function to stop safety stack
stop_safety_stack() {
    echo -e "${YELLOW}Stopping safety stack...${NC}"
    
    if [ -f /tmp/safety_stack.pid ]; then
        local pid=$(cat /tmp/safety_stack.pid)
        if ps -p $pid > /dev/null; then
            kill $pid || true
        fi
        rm /tmp/safety_stack.pid
    fi
    
    # Kill any remaining safety processes
    pkill -f "safety_envelope_node" || true
    pkill -f "safety_supervisor_node" || true
    pkill -f "safety_drive_bridge_node" || true
    pkill -f "ros2 launch" || true
    
    sleep 2
    echo -e "${GREEN}✓ Safety stack stopped${NC}"
}

# Function to validate safety nodes are running
validate_safety_nodes() {
    echo -e "${YELLOW}Checking safety nodes...${NC}"
    
    local nodes_found=0
    local nodes_total=0
    
    # Check for critical safety nodes
    local nodes=(
        "safety_envelope_node"
        "safety_supervisor_node"
        "safety_drive_bridge_node"
    )
    
    for node in "${nodes[@]}"; do
        nodes_total=$((nodes_total + 1))
        if ros2 node list 2>/dev/null | grep -q "$node"; then
            echo -e "${GREEN}✓ Node $node is running${NC}"
            nodes_found=$((nodes_found + 1))
        else
            echo -e "${YELLOW}⚠ Node $node not found${NC}"
        fi
    done
    
    if [ $nodes_found -eq 0 ]; then
        echo -e "${RED}✗ No safety nodes found - system may not be running${NC}"
        return 1
    fi
    
    echo -e "${GREEN}✓ Found $nodes_found/$nodes_total safety nodes${NC}"
}

# Function to validate safety topics
validate_safety_topics() {
    echo -e "${YELLOW}Checking safety topics...${NC}"
    
    local topics_found=0
    local topics_total=0
    
    local topics=(
        "/safety/state"
        "/safety/envelope_status"
        "/safety/safe_stop"
        "/cmd_vel_nav"
    )
    
    for topic in "${topics[@]}"; do
        topics_total=$((topics_total + 1))
        if ros2 topic list 2>/dev/null | grep -q "$topic"; then
            echo -e "${GREEN}✓ Topic $topic exists${NC}"
            topics_found=$((topics_found + 1))
        else
            echo -e "${YELLOW}⚠ Topic $topic not found${NC}"
        fi
    done
    
    if [ $topics_found -eq 0 ]; then
        echo -e "${RED}✗ No safety topics found - system may not be running${NC}"
        return 1
    fi
    
    echo -e "${GREEN}✓ Found $topics_found/$topics_total safety topics${NC}"
}

# Function to run quick demo validation
validate_quick_demo() {
    echo -e "${YELLOW}Running quick demo validation...${NC}"
    
    # Launch safety stack first
    launch_safety_stack
    
    # Wait a bit more for system to be fully ready
    sleep 5
    
    # Validate infrastructure
    validate_safety_nodes || { stop_safety_stack; return 1; }
    validate_safety_topics || { stop_safety_stack; return 1; }
    
    # Run quick demo with timeout and capture output
    echo -e "${YELLOW}Starting quick demo...${NC}"
    timeout 90s python3 /workspace/ros2_ws/src/safety_core_bringup/scripts/quick_demo.py > /tmp/quick_demo.log 2>&1
    local demo_exit_code=$?
    
    # Stop safety stack
    stop_safety_stack
    
    # Check demo result
    if [ $demo_exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ Quick demo completed successfully${NC}"
        return 0
    elif [ $demo_exit_code -eq 124 ]; then
        echo -e "${YELLOW}⚠ Quick demo timed out (90s)${NC}"
        echo -e "${YELLOW}This may indicate slow performance or hanging${NC}"
        return 1
    else
        echo -e "${RED}✗ Quick demo failed with exit code $demo_exit_code${NC}"
        echo -e "${RED}Log output:${NC}"
        cat /tmp/quick_demo.log
        return 1
    fi
}

# Function to run comprehensive demo validation
validate_comprehensive_demo() {
    echo -e "${YELLOW}Running comprehensive demo validation...${NC}"
    
    # Launch safety stack first
    launch_safety_stack
    
    # Wait a bit more for system to be fully ready
    sleep 5
    
    # Validate infrastructure
    validate_safety_nodes || { stop_safety_stack; return 1; }
    validate_safety_topics || { stop_safety_stack; return 1; }
    
    # Run comprehensive demo with timeout and capture output
    echo -e "${YELLOW}Starting comprehensive demo...${NC}"
    timeout 240s python3 /workspace/ros2_ws/src/safety_core_bringup/scripts/comprehensive_demo.py > /tmp/comprehensive_demo.log 2>&1
    local demo_exit_code=$?
    
    # Stop safety stack
    stop_safety_stack
    
    # Check demo result
    if [ $demo_exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ Comprehensive demo completed successfully${NC}"
        return 0
    elif [ $demo_exit_code -eq 124 ]; then
        echo -e "${YELLOW}⚠ Comprehensive demo timed out (240s)${NC}"
        echo -e "${YELLOW}This may indicate slow performance or hanging${NC}"
        return 1
    else
        echo -e "${RED}✗ Comprehensive demo failed with exit code $demo_exit_code${NC}"
        echo -e "${RED}Log output:${NC}"
        cat /tmp/comprehensive_demo.log
        return 1
    fi
}

# Function to validate launch file
validate_launch_file() {
    local launch_file=$1
    echo -e "${YELLOW}Validating launch file: $launch_file${NC}"
    
    # Kill any existing processes
    pkill -f "ros2 launch" || true
    sleep 2
    
    # Launch in background with logging
    timeout 20s ros2 launch $launch_file > /tmp/launch_validation.log 2>&1 &
    local launch_pid=$!
    
    # Wait for launch to start
    sleep 10
    
    # Check if launch process is still running
    if ps -p $launch_pid > /dev/null; then
        echo -e "${GREEN}✓ Launch file $launch_file started successfully${NC}"
        
        # Validate nodes
        validate_safety_nodes || { pkill -f "ros2 launch" || true; return 1; }
        
        # Validate topics
        validate_safety_topics || { pkill -f "ros2 launch" || true; return 1; }
        
        # Clean up
        pkill -f "ros2 launch" || true
        wait $launch_pid 2>/dev/null || true
        
        echo -e "${GREEN}✓ Launch file validation completed${NC}"
        return 0
    else
        echo -e "${RED}✗ Launch file $launch_file failed to start${NC}"
        echo -e "${RED}Log output:${NC}"
        cat /tmp/launch_validation.log
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
            validate_quick_demo || exit 1
            echo ""
            validate_comprehensive_demo || exit 1
            echo ""
            validate_launch_file "safety_core_bringup safety_sim.launch.py" || exit 1
            echo ""
            validate_launch_file "safety_core_bringup diagnostics.launch.py" || exit 1
            ;;
        *)
            echo -e "${RED}Error: Unknown validation mode: $1${NC}"
            echo "Usage: $0 [quick|comprehensive|launch <file>|all]"
            exit 1
            ;;
    esac
    
    echo -e "${GREEN}=== Demo validation completed successfully ===${NC}"
}

# Cleanup on exit
trap stop_safety_stack EXIT

# Run main function
main "$@"
