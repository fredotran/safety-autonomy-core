# Devin Repository Rules for Safety Autonomy Core

## Auto-Loaded Skills

The following skills are automatically loaded when working in this repository to provide specialized assistance for ROS 2 robotics, sensor fusion, localization, and safety systems.

### Primary Skills (Always Loaded)

- **ros-robotics-expert**: ROS 2 (Robot Operating System) for robotics arms and mobile robotics
  - ROS 2 node development and debugging
  - Launch file configuration
  - Topic/service/action integration
  - ROS 2 navigation stack (Nav2)
  - SLAM and localization

- **fusion-filter-robotics-expert**: Fusion filters and sensor fusion for robotics localization
  - Kalman filters (EKF, UKF)
  - Multi-sensor fusion (LiDAR, IMU, GPS, odometry)
  - Sensor fusion debugging and optimization
  - GPS/INS integration
  - Filter tuning and validation

- **robotics-localization-expert**: Robotics localization problems
  - Visual odometry and SLAM
  - GPS-based localization
  - Sensor fusion for positioning
  - Pose estimation
  - Drift correction

- **robotics-odometry-expert**: Robotics odometry systems
  - Wheel odometry
  - Visual odometry
  - Underwater odometry
  - Odometry calibration

- **robotics-data-analyzer**: Robotics sensor data analysis
  - Sensor data processing and visualization
  - Telemetry analysis
  - Hardware performance diagnostics
  - Sensor calibration

### Secondary Skills (Context-Dependent)

- **gps-ins-localization-expert**: GPS and INS integration for localization
  - GPS/INS sensor fusion
  - GPS denial handling
  - Multipath detection
  - Sensor calibration

- **robotics-devops-engineer**: Robotics deployment, containerization, and automation
  - CI/CD pipeline setup for robotics
  - Docker containerization for ROS 2
  - Kubernetes orchestration for robot fleets
  - Infrastructure as Code with Ansible
  - Production deployment strategies
  - Automated testing and deployment

- **cpp-pro**: C and C++ code review and optimization
  - Safety-critical C++ code review
  - Performance optimization
  - Memory management
  - Security analysis

- **python-expert**: Python code assistance
  - ROS 2 Python nodes
  - Testing scripts
  - Data analysis scripts
  - Code optimization

## Project Context

This is a safety autonomy core project for industrial AGVs (Automated Guided Vehicles) with:

- **ROS 2 Jazzy** integration
- **Multi-sensor fusion** localization (EKF with wheel odometry, IMU, GPS, LiDAR)
- **Safety-critical systems** with envelope monitoring and emergency stopping
- **Industrial warehouse simulation** in Gazebo Harmonic
- **Nav2 navigation stack** integration
- **C++ library** with ROS 2 wrapper nodes
- **GitHub Actions CI/CD** for automated testing and release management
- **GitLab integration** with remote repository

## Common Workflows

### ROS 2 Development
- Use `ros-robotics-expert` for ROS 2 node development, launch files, and integration
- Use `cpp-pro` for C++ node implementation review
- Use `python-expert` for Python test scripts and utilities

### Localization and Sensor Fusion
- Use `fusion-filter-robotics-expert` for EKF/UKF implementation and tuning
- Use `robotics-localization-expert` for localization architecture and debugging
- Use `gps-ins-localization-expert` for GPS/INS integration issues
- Use `robotics-odometry-expert` for odometry calibration and drift analysis

### Sensor Data Analysis
- Use `robotics-data-analyzer` for processing sensor logs and telemetry
- Use `fusion-filter-robotics-expert` for sensor fusion validation
- Use `robotics-localization-expert` for localization accuracy analysis

### Safety Systems
- Use `ros-robotics-expert` for safety node integration with ROS 2
- Use `cpp-pro` for safety-critical code review
- Use `robotics-data-analyzer` for safety event analysis

### DevOps and Deployment
- Use `robotics-devops-engineer` for CI/CD pipeline setup
- Use `robotics-devops-engineer` for Docker containerization of ROS 2 applications
- Use `robotics-devops-engineer` for Kubernetes orchestration of robot fleets
- Use `robotics-devops-engineer` for automated deployment strategies
- Use `robotics-devops-engineer` for infrastructure as Code with Ansible

## Repository Structure

```
safety-autonomy-core/
├── .github/workflows/            # GitHub Actions CI/CD
├── .devin/                        # Devin configuration
├── ros2/                          # ROS 2 workspace
│   ├── src/
│   │   ├── safety_core_msgs/      # ROS 2 messages
│   │   ├── safety_core_ros/       # C++ wrapper nodes
│   │   ├── safety_core_nav2/      # Nav2 behavior tree plugin
│   │   ├── safety_core_sim/       # Simulation (Gazebo, URDF, SDF)
│   │   └── safety_core_bringup/   # Launch files, configs, RViz
│   └── LOCALIZATION.md            # Localization system docs
└── include/safety_core/          # C++ safety library
```

## Key Configuration Files

- **ROS 2 Launch:** `ros2/src/safety_core_bringup/launch/`
- **EKF Config:** `ros2/src/safety_core_bringup/config/ekf_config.yaml`
- **Safety Params:** `ros2/src/safety_core_bringup/config/safety_params.yaml`
- **Nav2 Config:** `ros2/src/safety_core_bringup/config/nav2_params.yaml`
- **URDF Model:** `ros2/src/safety_core_sim/description/agv.urdf.xacro`
- **Gazebo Bridge:** `ros2/src/safety_core_sim/config/ros_gz_bridge.yaml`
- **CI/CD Workflows:** `.github/workflows/` (GitHub Actions)
- **Devin Config:** `.devin/rules.md` (Devin skills and rules)

## Testing and Validation

- **Unit Tests:** C++ library tests in `test/`
- **Integration Tests:** ROS 2 test scripts in `ros2/src/safety_core_bringup/scripts/`
- **Localization Tests:** `test_localization.py` for EKF sensor fusion
- **Demo Scripts:** Various demo scripts in `ros2/src/safety_core_bringup/scripts/`

## Build and Run

```bash
# Build ROS 2 workspace
cd ros2
colcon build

# Source workspace
source install/setup.bash

# Run full demo
ros2 launch safety_core_bringup agv_warehouse.launch.py

# Run with EKF localization
ros2 launch safety_core_bringup agv_warehouse.launch.py ekf:=true
```

## Troubleshooting

For common issues:
- **ROS 2 problems:** Use `ros-robotics-expert`
- **Localization drift:** Use `fusion-filter-robotics-expert` or `robotics-localization-expert`
- **Sensor data issues:** Use `robotics-data-analyzer`
- **GPS problems:** Use `gps-ins-localization-expert`
- **C++ compilation errors:** Use `cpp-pro`
- **Python script errors:** Use `python-expert`
- **CI/CD pipeline issues:** Use `robotics-devops-engineer`
- **Docker containerization:** Use `robotics-devops-engineer`
- **Kubernetes deployment:** Use `robotics-devops-engineer`
- **Infrastructure automation:** Use `robotics-devops-engineer`

## Documentation

- **Main README:** `ros2/README.md`
- **Localization Guide:** `ros2/LOCALIZATION.md`
- **Implementation Summary:** `ros2/LOCALIZATION_IMPLEMENTATION_SUMMARY.md`
- **Demo Guide:** `ros2/DEMO_GUIDE.md`
- **Comprehensive Demo:** `ros2/COMPREHENSIVE_DEMO.md`
