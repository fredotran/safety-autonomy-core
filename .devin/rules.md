# Devin Repository Rules

## Auto-Loaded Skills

The following skills are automatically loaded for this project:

### Robotics Development
- **ros-robotics-expert**: ROS (Robot Operating System) for robotics arms and mobile robotics
- **fusion-filter-robotics-expert**: Fusion filters, sensor fusion, and localization challenges
- **robotics-localization-expert**: Robotics localization problems including visual odometry, SLAM, GPS-based localization
- **robotics-odometry-expert**: Robotics odometry problems including visual odometry, ground-based odometry
- **robotics-data-analyzer**: Analyze, debug, or diagnose issues with robotics sensor data and perception systems
- **gps-ins-localization-expert**: GPS and INS (Inertial Navigation System) integration for robotics localization
- **robotics-devops-engineer**: Deploy, containerize, or automate robotics code for production environments

### Code Quality
- **cpp-pro**: Review C or C++ code for bugs, security issues, logic errors, or performance problems
- **python-expert**: Help with Python code, debugging, optimization, or best practices

## Project Context

### Technology Stack
- **ROS 2**: Jazzy (latest ROS 2 distribution)
- **C++**: C++20 with safety-critical coding standards
- **Simulation**: Gazebo Harmonic for industrial AGV simulation
- **Navigation**: Nav2 for autonomous navigation
- **Localization**: EKF-based multi-sensor fusion (LiDAR, IMU, GPS, odometry)

### Project Structure
- **C++ Library**: Safety-critical autonomy core at repo root
- **ROS 2 Wrapper**: ROS 2 integration in `ros2/` directory
- **Simulation**: Industrial warehouse AGV simulation
- **CI/CD**: GitHub Actions with clang-tidy, ros2_lint, and release automation

### Safety-Critical Nature
This project implements safety-critical systems for industrial AGVs. Code changes require:
- Rigorous testing and validation
- Safety envelope verification
- Sensor fusion integrity
- Real-time performance guarantees

## Workflows

### ROS 2 Development
When working on ROS 2 components:
1. Use `ros-robotics-expert` for ROS 2 architecture and best practices
2. Test with `ros2/src/safety_core_bringup/launch/` files
3. Verify sensor integration with Gazebo simulation
4. Check message passing and topic connectivity

### Sensor Analysis and Debugging
When analyzing sensor data or debugging perception issues:
1. Use `robotics-data-analyzer` for log analysis and diagnostics
2. Use `fusion-filter-robotics-expert` for sensor fusion issues
3. Use `gps-ins-localization-expert` for GPS/IMU integration problems
4. Verify with ROS bag files and telemetry data

### Localization and Navigation
When working on localization or navigation:
1. Use `robotics-localization-expert` for localization architecture
2. Use `robotics-odometry-expert` for odometry systems
3. Use `fusion-filter-robotics-expert` for EKF and sensor fusion
4. Test with multi-sensor scenarios (GPS denial, sensor failure)

### Safety Systems
When working on safety-critical components:
1. Use `cpp-pro` for rigorous C++ code review
2. Verify safety envelope calculations
3. Test fault injection scenarios
4. Validate real-time constraints

### DevOps and Deployment
When deploying or containerizing:
1. Use `robotics-devops-engineer` for production deployment
2. Configure Docker/Kubernetes for robotics workloads
3. Set up CI/CD pipelines for automated testing
4. Implement automated rollback procedures

## Branch Protection and Workflow

### Pull Request Policy
- **ALWAYS create a pull request** for any changes to the main branch
- Never push directly to main for:
  - New features
  - Bug fixes
  - CI/CD changes
  - Documentation updates
  - Configuration changes
  - Any code changes

### Branch Naming Convention
- Use descriptive branch names:
  - `feat/` for new features
  - `fix/` for bug fixes
  - `ci/` for CI/CD changes
  - `docs/` for documentation updates
  - `refactor/` for code refactoring
  - `chore/` for maintenance tasks

### Workflow
1. Create a new branch from main
2. Make changes on the feature branch
3. Commit changes with descriptive messages
4. Push branch to remote
5. Create a pull request
6. Wait for CI checks to pass
7. Request review if needed
8. Merge pull request after approval

### Commit Message Format
Use conventional commits:
- `feat:` for new features
- `fix:` for bug fixes
- `ci:` for CI/CD changes
- `docs:` for documentation
- `refactor:` for code refactoring
- `chore:` for maintenance tasks
- `test:` for test changes

## Troubleshooting Guide

### Build Issues
- C++ compilation errors: Use `cpp-pro` for code analysis
- ROS 2 build failures: Use `ros-robotics-expert` for dependency issues
- CMake configuration problems: Check `CMakeLists.txt` and package dependencies

### Localization Issues
- Robot drifting: Use `robotics-localization-expert` for drift diagnosis
- GPS loss handling: Use `gps-ins-localization-expert` for GPS denial scenarios
- Sensor fusion divergence: Use `fusion-filter-robotics-expert` for EKF tuning

### Sensor Issues
- LiDAR noise: Use `robotics-data-analyzer` for sensor data analysis
- IMU calibration: Use `gps-ins-localization-expert` for IMU integration
- Odometry errors: Use `robotics-odometry-expert` for odometry diagnosis

### Performance Issues
- Real-time violations: Use `cpp-pro` for performance optimization
- Memory leaks: Use `cpp-pro` for memory analysis
- CPU bottlenecks: Profile and optimize critical paths
