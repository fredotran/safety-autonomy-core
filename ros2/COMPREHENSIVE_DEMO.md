# Comprehensive Safety Autonomy Core Demo

This comprehensive demo showcases all key capabilities of the safety_autonomy_core library through a choreographed sequence of maneuvers in the industrial warehouse simulation.

## Demo Overview

The comprehensive demo demonstrates:

### Core Safety Features

1. **State Machine Transitions** - Init → Idle → Moving → AvoidingObstacle → SafeStop
2. **Safety Envelope Zones** - Clear → Warning → Protective → Emergency zone detection
3. **Speed Limiting** - Automatic speed reduction in Warning zone
4. **Mode Escalation** - Automatic transitions based on zone changes
5. **Fault Latching** - Safety fault latching on critical conditions
6. **Fault Recovery** - Fault clearing and system reset
7. **Jerk-Limited Stopping** - Smooth emergency stop profiles
8. **Command Freshness Watchdog** - Stale command detection
9. **Localization Monitoring** - Odometry staleness detection
10. **Complex Maneuvers** - Multi-segment paths with zone transitions

### Edge Cases and Real-World Scenarios

11. **Sensor Failures** - Lidar drop simulation and rapid zone transitions
12. **Command Failures** - Stale command detection and burst recovery
13. **Boundary Conditions** - Exact threshold crossings and zero-distance obstacles
14. **Dynamic Obstacles** - Sudden obstacle appearance and zone oscillations
15. **Extreme Values** - Maximum speed, angular velocity, and zero commands
16. **Conflicting Conditions** - Simultaneous safety conditions and conflicting motion
17. **Recovery Scenarios** - Degraded operation recovery
18. **Timing Scenarios** - Fast obstacle approach and slow creep detection
19. **System Stress** - Rapid mode transitions and stability testing

## Running the Demo

### Quick Start

```bash
cd /home/itsfredostark/Code/safety-autonomy-core
./setup_demo.sh
```

Select option 1 (Full demo) to launch the comprehensive demo with Gazebo, safety stack, and RViz.

### Manual Launch

```bash
# Source ROS 2 and workspace
source /opt/ros/jazzy/setup.bash
source ros2/install/setup.bash

# Launch the comprehensive demo
ros2 launch safety_core_bringup comprehensive_demo.launch.py
```

### Launch Options

| Argument | Default | Description |
|----------|---------|-------------|
| `use_sim_time` | `true` | Use Gazebo simulation time |
| `rviz` | `true` | Launch RViz for visualization |
| `run_demo` | `true` | Run the automated demo script |

**Examples:**

```bash
# Launch without RViz
ros2 launch safety_core_bringup comprehensive_demo.launch.py rviz:=false

# Launch without running automated demo (manual control only)
ros2 launch safety_core_bringup comprehensive_demo.launch.py run_demo:=false
```

### Standalone Demo Script

If you already have the simulation running, you can run the demo script separately:

```bash
source /opt/ros/jazzy/setup.bash
source ros2/install/setup.bash
python3 ros2/src/safety_core_bringup/scripts/comprehensive_demo.py
```

## Demo Sequence

The automated demo performs the following sequence:

### Section 1: Initial State and Normal Operation
- Verifies initial safety state (Idle mode, Clear zone)
- Demonstrates normal driving in Clear zone
- Performs basic maneuvers (straight, turns)

### Section 2: Speed Limiting in Warning Zone
- Drives toward obstacles to trigger Warning zone
- Demonstrates automatic speed limit reduction
- Shows system response to approaching obstacles

### Section 3: Protective Zone and Obstacle Avoidance
- Continues approach to trigger Protective zone
- Demonstrates transition to AvoidingObstacle mode
- Shows system's response to close obstacles

### Section 4: Emergency Zone and Emergency Stop
- Aggressive approach to trigger Emergency zone
- Demonstrates emergency zone detection
- Shows fault latching and SafeStop engagement

### Section 5: Fault Latching and Recovery
- Checks fault state after emergency conditions
- Attempts to drive with latched fault (should fail)
- Demonstrates fault clearing and system recovery

### Section 6: Jerk-Limited Emergency Stop
- Drives at speed then initiates emergency stop
- Demonstrates smooth deceleration profile
- Shows jerk-limited stopping behavior

### Section 7: Command Freshness Watchdog
- Stops command publication to trigger watchdog
- Demonstrates stale command detection
- Shows system response to missing commands

### Section 8: Localization Staleness Monitoring
- Explains localization timeout monitoring
- Shows threshold configuration (0.5s)
- Describes LocalizationLost mode

### Section 9: Complex Maneuver Sequence
- Executes complex path with multiple turns
- Demonstrates multiple zone transitions
- Shows system handling of complex scenarios

### Section 10: Return to Idle
- Returns robot to safe position
- Demonstrates return to Idle mode
- Shows final safety state

### Section 11: Sensor Failures
- Simulates lidar drop (sensor failure)
- Tests rapid zone transitions (Clear → Emergency)
- Demonstrates system response to missing sensor data

### Section 12: Command Failures
- Tests stale command detection
- Demonstrates command freshness watchdog
- Shows command burst recovery after timeout

### Section 13: Boundary Conditions
- Tests exact threshold crossing behavior
- Demonstrates zero-distance obstacle handling
- Shows system response at boundary values

### Section 14: Dynamic Obstacles
- Simulates sudden obstacle appearance
- Tests zone oscillation handling
- Demonstrates system stability under dynamic conditions

### Section 15: Extreme Values
- Tests maximum speed command enforcement
- Demonstrates maximum angular velocity handling
- Shows zero command processing

### Section 16: Conflicting Conditions
- Tests simultaneous safety conditions
- Demonstrates conflicting motion scenarios
- Shows system prioritization logic

### Section 17: Recovery Scenarios
- Tests degraded operation recovery
- Demonstrates system recovery to normal operation
- Shows transition from degraded to normal mode

### Section 18: Timing Scenarios
- Tests very fast obstacle approach
- Demonstrates slow creep into danger zone
- Shows system response to different timing scenarios

### Section 19: System Stress
- Tests rapid mode transitions
- Demonstrates system stability under stress
- Shows handling of rapid state changes

### Section 20: Return to Idle and Final State
- Returns robot to safe position
- Demonstrates final state verification
- Shows comprehensive summary of all capabilities

## What You'll See

### Console Output
The demo script provides detailed console output showing:
- Safety state changes (mode, zone, fault status)
- Envelope status updates (distance, speed limit)
- Safe stop notifications
- Demo section headers and descriptions
- Real-time system status

### RViz Visualization
When RViz is launched, you'll see:
- AGV robot model in the warehouse
- Safety zone markers (Clear, Warning, Protective, Emergency)
- Laser scan visualization
- Robot trajectory and current position
- Safety state indicators

### Gazebo Simulation
The Gazebo window shows:
- Industrial warehouse with shelves, pallets, forklift
- AGV robot moving through the environment
- Realistic physics and sensor simulation
- Animated worker in the environment

## Customizing the Demo

You can modify the demo behavior by editing `comprehensive_demo.py`:

```python
# Movement parameters
self.linear_speed = 0.8  # m/s (forward speed)
self.angular_speed = 0.6  # rad/s (turning speed)

# Maneuver parameters
self.drive_straight(distance, speed)  # Drive straight
self.rotate(angle, speed)             # Rotate
```

## Safety Features Demonstrated

### State Machine
- **Init**: Initial state on startup
- **Idle**: Ready state, no movement
- **Moving**: Active movement in Clear zone
- **Degraded**: Reduced capability mode
- **AvoidingObstacle**: Navigating around obstacles
- **LocalizationLost**: Odometry timeout
- **Docking**: Docking operation
- **SafeStop**: Emergency stop state

### Edge Cases and Real-World Scenarios
- **Sensor Failure Handling**: System response to missing sensor data
- **Rapid Zone Transitions**: Fast escalation between safety zones
- **Stale Command Detection**: Command freshness watchdog activation
- **Boundary Condition Handling**: Exact threshold and zero-distance scenarios
- **Dynamic Obstacle Response**: Sudden obstacle appearance and oscillation
- **Extreme Value Enforcement**: Speed and angular velocity limits
- **Conflicting Condition Resolution**: Multiple simultaneous safety conditions
- **Degraded Operation Recovery**: System recovery from reduced capability
- **Timing Scenario Handling**: Fast approach and slow creep detection
- **System Stress Testing**: Rapid mode transitions and stability

### Safety Envelope Zones
- **Clear**: Full speed allowed (distance > warning threshold)
- **Warning**: Speed limited (distance > protective threshold)
- **Protective**: Controlled deceleration (distance > emergency threshold)
- **Emergency**: Immediate stop (distance <= emergency threshold)

### Fault Handling
- Fault latching on critical conditions
- Automatic mode escalation
- Manual fault clearing required
- System reset after fault clearance

### Command Gating
- Speed limiting based on zone
- Freshness watchdog (0.25s timeout)
- Jerk-limited emergency stop
- Zero command on fault latched

## Troubleshooting

### Robot Not Moving
1. Check safety state: `ros2 topic echo /safety/state`
2. Check if fault is latched
3. Verify envelope node is running: `ros2 node list`
4. Check for obstacles in Emergency zone

### Demo Script Not Starting
1. Ensure simulation is running
2. Check Python script permissions: `chmod +x scripts/comprehensive_demo.py`
3. Verify ROS 2 environment is sourced
4. Check for Python errors in console

### RViz Not Showing Robot
1. Check RViz is launched
2. Verify TF tree is publishing: `ros2 topic echo /tf`
3. Check robot description: `ros2 topic echo /robot_description`
4. Verify fixed frame in RViz matches simulation

## Integration with Safety Core Library

The demo integrates with the following safety_core components:

- **safety_core::sm::ModeStateMachine** - State machine management
- **safety_core::safety::SafetySupervisor** - Safety escalation logic
- **safety_core::safety::evaluate_stop_distance** - Zone evaluation
- **safety_core::motion::generate_jerk_limited_stop_profile** - Emergency stop
- **safety_core::platform::DriveActuator** - Command freshness watchdog
- **safety_core::diag::DiagnosticTransport** - Event publishing

## Performance Characteristics

The demo runs in real-time with the following characteristics:
- State machine updates: 100 Hz
- Envelope evaluation: 15 Hz (lidar rate)
- Command publishing: 10 Hz (demo script)
- Safety state publishing: 10 Hz
- Gazebo simulation: 50-100 Hz (depends on hardware)
- Total demo duration: ~5-7 minutes (20 sections with edge cases)

## Next Steps

After completing the comprehensive demo:

1. **Test with Nav2**: Enable autonomous navigation
   ```bash
   ros2 launch safety_core_bringup agv_warehouse.launch.py nav2:=true
   ```

2. **Custom Scenarios**: Modify the demo script for your use case

3. **Parameter Tuning**: Adjust safety parameters in `safety_params.yaml`

4. **Hardware Testing**: Use with physical AGV and real sensors

5. **Integration**: Integrate with your existing autonomy stack

## Support

For issues or questions about the comprehensive demo:
- Check the console output for error messages
- Review the safety state and envelope status topics
- Consult the main README for troubleshooting
- Check the PATH_FOLLOWING.md for manual control options
