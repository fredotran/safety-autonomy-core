# Path Following Scripts

This directory contains Python scripts for making the AGV follow various movement patterns in the simulation.

## Prerequisites

The ROS 2 simulation must be running with the safety stack:

```bash
cd /home/itsfredostark/Code/safety-autonomy-core
./setup_demo.sh
# Select option 1 (Full demo) or option 2 (Safety stack only)
```

## Available Scripts

### 1. path_follower.py - Simple Square Pattern

Basic script that makes the robot drive in a square pattern.

```bash
source /opt/ros/jazzy/setup.bash
source ros2/install/setup.bash
python3 ros2/path_follower.py
```

**Parameters (edit in script):**
- `linear_speed`: 0.5 m/s (forward speed)
- `angular_speed`: 0.5 rad/s (turning speed)
- `side_length`: 2.0 m (length of each side)

### 2. path_patterns.py - Multiple Patterns

Advanced script with multiple movement patterns and command-line options.

```bash
source /opt/ros/jazzy/setup.bash
source ros2/install/setup.bash
python3 ros2/path_patterns.py --pattern square --reps 2
python3 ros2/path_patterns.py --pattern circle --reps 1
python3 ros2/path_patterns.py --pattern figure8 --reps 1
python3 ros2/path_patterns.py --pattern manual
```

**Patterns:**
- `square`: Drive in a square (default)
- `circle`: Drive in circles
- `figure8`: Drive in a figure-8 pattern
- `manual`: Forward, turn 180°, drive back

**Options:**
- `--pattern`: Choose the movement pattern
- `--reps`: Number of repetitions (default: 2)

**Parameters (edit in script):**
- `linear_speed`: 0.5 m/s
- `angular_speed`: 0.5 rad/s
- `radius`: 1.0 m (for circle and figure-8)

## Manual Control

For direct manual control, publish velocity commands:

```bash
# Continuous velocity command
ros2 topic pub --rate 10 /cmd_vel_nav geometry_msgs/Twist "{linear: {x: 0.5}, angular: {z: 0.0}}"

# Single command
ros2 topic pub --once /cmd_vel_nav geometry_msgs/Twist "{linear: {x: 0.5}, angular: {z: 0.0}}"
```

**Command format:**
- `linear.x`: Forward/backward speed (m/s)
- `angular.z`: Rotational speed (rad/s)

## Safety Considerations

The safety system will automatically:
- Stop the robot if it enters the Protective or Emergency zones
- Limit speed based on the current safety zone
- Engage jerk-limited stopping when safe_stop is requested
- Latch faults if critical conditions are violated

If the robot stops unexpectedly:
1. Check the safety state: `ros2 topic echo /safety/state`
2. Check the envelope status: `ros2 topic echo /safety/envelope_status`
3. Clear latched faults by restarting the safety nodes

## Integration with Nav2

For autonomous navigation, enable Nav2 in the launch:

```bash
ros2 launch safety_core_bringup agv_warehouse.launch.py nav2:=true
```

Then use RViz to set navigation goals. The safety system will automatically gate Nav2 commands through the safety envelope.
