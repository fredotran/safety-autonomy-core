# Project Rules

## Auto-Loaded Skills

### Robotics
- ros-robotics-expert: ROS 2 development
- fusion-filter-robotics-expert: Sensor fusion and localization
- robotics-localization-expert: SLAM, GPS, pose estimation
- robotics-odometry-expert: Visual/ground odometry
- robotics-data-analyzer: Sensor data analysis
- gps-ins-localization-expert: GPS/INS integration
- robotics-devops-engineer: Deployment and CI/CD

### Code Quality
- cpp-pro: C/C++ code review
- python-expert: Python code review

## Project Context

- **ROS 2**: Jazzy
- **C++**: C++20 with safety-critical standards
- **Simulation**: Gazebo Harmonic
- **Navigation**: Nav2
- **Localization**: EKF multi-sensor fusion (LiDAR, IMU, GPS, odometry)

## Branch Policy

- **ALWAYS create pull requests** for main branch changes
- Never push directly to main for code changes
- Branch naming: `feat/`, `fix/`, `ci/`, `docs/`, `refactor/`, `chore/`
- Auto-merge to main when CI passes for non-breaking changes
- Manual review required for major features and breaking changes

## Commit Guidelines

- Use conventional commits: `feat:`, `fix:`, `ci:`, `docs:`, `refactor:`, `chore:`, `test:`
- Do NOT include Devin co-authoring in commits
- User is the only author
- Keep commits focused and atomic
- Use present tense in commit messages

## Workflows

- ROS 2 development: Use ros-robotics-expert
- Sensor analysis: Use robotics-data-analyzer
- Localization: Use robotics-localization-expert
- Safety systems: Use cpp-pro for code review
- DevOps: Use robotics-devops-engineer
