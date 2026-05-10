# Docker Deployment Guide

This guide explains how to run the Safety Autonomy Core ROS2 demo using Docker containers, which allows you to run the demo on systems without a local ROS2 installation.

## Architecture Overview

### Multi-Stage Docker Build

The Dockerfile uses a multi-stage build following DevOps best practices:

1. **Base Stage**: ROS2 Jazzy perception image + build dependencies
2. **Development Stage**: Full workspace with build artifacts and development tools  
3. **Runtime Stage**: Optimized production image with only runtime dependencies

### Production-Ready Features

- **Resource Management**: CPU/memory limits for real-time constraints
- **Health Monitoring**: ROS2 node health checks
- **Security Scanning**: Vulnerability scanning support
- **Rollback Support**: Multi-stage builds for safe deployments
- **Observability**: Comprehensive logging and monitoring hooks

## Overview

The Docker setup provides:
- **Complete ROS2 Jazzy environment** with all dependencies pre-installed
- **Gazebo Harmonic** for simulation
- **Nav2** for navigation stack
- **All safety core components** in a containerized environment
- **Display forwarding** for GUI applications (Gazebo, RViz)
- **Hardware acceleration** support for GPU-based rendering
- **Production-grade** resource management and health checks

## Prerequisites

### Host System Requirements

- **Docker Engine** 20.10+ with Docker Compose plugin
- **NVIDIA Docker runtime** (for GPU acceleration with Gazebo)
- **X11 server** running (for display forwarding)
- **Ubuntu 20.04+** or other Linux distribution (Docker on Mac/Windows may require additional configuration)

### Install Docker

```bash
# Install Docker Engine on Ubuntu
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# Add your user to the docker group
sudo usermod -aG docker $USER

# Log out and log back in for group changes to take effect
```

### Install NVIDIA Docker Runtime (for GPU support)

```bash
# Install NVIDIA Container Toolkit
distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/nvidia-docker/gpgkey | sudo apt-key add -
curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.list | \
  sudo tee /etc/apt/sources.list.d/nvidia-docker.list

sudo apt-get update
sudo apt-get install -y nvidia-container-toolkit
sudo systemctl restart docker
```

## Quick Start

### Using the setup script (recommended)

```bash
# Docker is now the default mode
./setup_demo.sh

# Or explicitly specify Docker mode
./setup_demo.sh --docker

# For local ROS2 installation (if you have ROS2 installed locally)
./setup_demo.sh --local
```

### Using Docker Compose directly

```bash
# Build the Docker image
docker compose build

# Run the full demo
docker compose run --rm safety-autonomy-demo \
    bash -c "source install/setup.bash && ros2 launch safety_core_bringup agv_warehouse.launch.py"
```

## Docker Architecture

### Multi-Stage Build

The Dockerfile uses a multi-stage build for optimization:

1. **Base Stage**: Ubuntu 24.04 with ROS2 Jazzy, Gazebo Harmonic, and all dependencies
2. **Development Stage**: Full workspace with build artifacts and development tools
3. **Runtime Stage**: Optimized image with only runtime dependencies (smaller size)

### Container Configuration

The Docker setup includes:

- **Display Forwarding**: X11 socket mounting for GUI applications
- **GPU Support**: NVIDIA runtime for hardware-accelerated rendering
- **Volume Mounts**: Workspace mounting for development and persistent storage
- **Network Mode**: Host networking for ROS2 communication
- **Privileged Mode**: For hardware access (sensors, serial devices)

## Usage Options

### 1. Full Demo

Launch the complete demo with Gazebo simulation, safety stack, Nav2, and RViz:

```bash
docker compose run --rm safety-autonomy-demo \
    bash -c "source install/setup.bash && ros2 launch safety_core_bringup agv_warehouse.launch.py"
```

### 2. Safety Stack with Simulation

Launch the safety stack with Gazebo simulation (no Nav2):

```bash
docker compose run --rm safety-autonomy-demo \
    bash -c "source install/setup.bash && ros2 launch safety_core_bringup safety_sim.launch.py"
```

### 3. Safety Stack Only

Launch only the safety stack (for physical robot integration):

```bash
docker compose run --rm safety-autonomy-demo \
    bash -c "source install/setup.bash && ros2 launch safety_core_bringup safety_only.launch.py"
```

### 4. Simulation Only

Launch only Gazebo simulation (no safety stack):

```bash
docker compose run --rm safety-autonomy-demo \
    bash -c "source install/setup.bash && ros2 launch safety_core_sim sim_only.launch.py"
```

### 5. Interactive Shell

Launch an interactive bash shell in the container:

```bash
docker compose run --rm safety-autonomy-demo
```

## Display Forwarding

### Linux (X11)

The Docker setup automatically configures X11 forwarding for Linux hosts:

```bash
# The docker-compose.yml handles X11 socket mounting
# Ensure your DISPLAY environment variable is set
echo $DISPLAY  # Should show :0 or similar

# Run the demo
docker compose run --rm safety-autonomy-demo \
    bash -c "source install/setup.bash && ros2 launch safety_core_bringup agv_warehouse.launch.py"
```

### Troubleshooting Display Issues

If Gazebo or RViz don't display:

```bash
# Allow local X11 connections
xhost +local:docker

# Verify DISPLAY is set
echo $DISPLAY

# Check X11 socket permissions
ls -la /tmp/.X11-unix/
```

## GPU Acceleration

### NVIDIA GPU Support

The Docker setup includes NVIDIA GPU support for Gazebo rendering:

```yaml
# From docker-compose.yml
deploy:
  resources:
    reservations:
      devices:
        - driver: nvidia
          count: all
          capabilities: [gpu]
```

### Verify GPU Access

```bash
# Check if NVIDIA runtime is available
docker run --rm --gpus all nvidia/cuda:11.6-base-ubuntu20.04 nvidia-smi

# Run Gazebo with GPU acceleration
docker compose run --rm --gpus all safety-autonomy-demo \
    bash -c "source install/setup.bash && ros2 launch safety_core_bringup agv_warehouse.launch.py"
```

### CPU-Only Mode

If you don't have an NVIDIA GPU or don't need GPU acceleration:

```bash
# Remove GPU configuration from docker-compose.yml
# Or run with CPU-only mode
docker compose run --rm safety-autonomy-demo \
    bash -c "source install/setup.bash && ros2 launch safety_core_bringup agv_warehouse.launch.py"
```

## Development Workflow

### Building the Image

```bash
# Build the development stage (includes build tools)
docker compose build

# Build specific stage
docker build --target development -t safety-autonomy-core:dev .
```

### Rebuilding After Changes

```bash
# Rebuild without cache
docker compose build --no-cache

# Rebuild specific service
docker compose build safety-autonomy-demo
```

### Volume Mounts for Development

The docker-compose.yml includes volume mounts for development:

```yaml
volumes:
  # Source code mounting
  - ./ros2:/workspace/ros2_ws/src/ros2:rw
  - ./include:/workspace/safety-autonomy-core/include:rw
  - ./src:/workspace/safety-autonomy-core/src:rw
  
  # Build artifacts
  - ros2-build:/workspace/ros2_ws/build
  - ros2-install:/workspace/ros2_ws/install
  - ros2-log:/workspace/ros2_ws/log
```

This allows you to:
- Edit source code on the host
- Rebuild inside the container
- Persist build artifacts across container restarts

### Building Inside Container

```bash
# Launch interactive shell
docker compose run --rm safety-autonomy-demo

# Inside container, rebuild workspace
cd /workspace/ros2_ws
colcon build --cmake-args -DSAFETY_CORE_ENABLE_AMENT=ON
source install/setup.bash
```

## Hardware Integration

### Serial Devices

For connecting to real hardware (sensors, actuators):

```yaml
# From docker-compose.yml
devices:
  - /dev/ttyUSB0:/dev/ttyUSB0
  - /dev/ttyACM0:/dev/ttyACM0
```

### USB Devices

```bash
# Pass through specific USB device
docker compose run --rm --device=/dev/bus/usb/001/002 safety-autonomy-demo

# Pass through all USB devices
docker compose run --rm --device=/dev/bus/usb safety-autonomy-demo
```

### Network Configuration

The container uses host networking for ROS2 communication:

```yaml
network_mode: host
```

This allows:
- ROS2 nodes to communicate with external ROS2 systems
- Zero-copy performance for sensor data
- Standard ROS2 DDS configuration

## Troubleshooting

### Docker Build Fails

```bash
# Check Docker daemon is running
sudo systemctl status docker

# Check disk space
df -h

# Clean up Docker cache
docker system prune -a
```

### Display Forwarding Issues

```bash
# Check X11 socket
ls -la /tmp/.X11-unix/

# Check DISPLAY variable
echo $DISPLAY

# Test X11 forwarding
docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix ubuntu xeyes
```

### GPU Not Available

```bash
# Check NVIDIA driver
nvidia-smi

# Check NVIDIA Docker runtime
docker run --rm --gpus all nvidia/cuda:11.6-base-ubuntu20.04 nvidia-smi

# Install NVIDIA Container Toolkit if missing
# See prerequisites section above
```

### Performance Issues

```bash
# Increase resource limits in docker-compose.yml
deploy:
  resources:
    limits:
      cpus: '8'
      memory: 16G

# Disable CPU scaling for better performance
sudo cpupower frequency-set -g performance
```

### Permission Issues

```bash
# Add user to docker group
sudo usermod -aG docker $USER

# Fix volume permissions
sudo chown -R $USER:$USER ./ros2/build
sudo chown -R $USER:$USER ./ros2/install
sudo chown -R $USER:$USER ./ros2/log
```

## Advanced Configuration

### Custom Docker Image

```bash
# Build with custom tag
docker build -t safety-autonomy-core:custom .

# Use custom image in docker-compose.yml
image: safety-autonomy-core:custom
```

### Environment Variables

```bash
# Set custom environment variables
docker compose run --rm -e ROS_DOMAIN_ID=42 safety-autonomy-demo

# Or add to docker-compose.yml
environment:
  - ROS_DOMAIN_ID=42
  - RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
```

### Custom Entrypoint

```bash
# Run custom command
docker compose run --rm safety-autonomy-demo \
    bash -c "source install/setup.bash && custom_command"

# Or modify entrypoint in docker-compose.yml
entrypoint: ["/custom/entrypoint.sh"]
```

## Production Deployment

### Using Runtime Stage

The runtime stage is optimized for production:

```bash
# Build runtime stage
docker build --target runtime -t safety-autonomy-core:runtime .

# Run runtime image
docker run --rm safety-autonomy-core:runtime
```

### Security Considerations

For production deployments:

1. **Remove privileged mode** if not needed
2. **Use specific device mappings** instead of `--device all`
3. **Run as non-root user** if possible
4. **Use read-only filesystem** where appropriate
5. **Implement resource limits** to prevent resource exhaustion
6. **Use network policies** instead of host networking
7. **Scan images for vulnerabilities** with tools like Trivy

```bash
# Example: Security-hardened run
docker run --rm \
    --read-only \
    --tmpfs /tmp \
    --cap-drop ALL \
    --cap-add NET_ADMIN \
    --security-opt=no-new-privileges \
    safety-autonomy-core:runtime
```

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Docker Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build Docker image
        run: docker compose build
      - name: Run tests
        run: docker compose run --rm safety-autonomy-demo bash -c "cd /workspace/ros2_ws && colcon test"
```

### Automated Deployment

```bash
# Build and push to registry
docker build -t registry.example.com/safety-autonomy-core:${GITHUB_SHA} .
docker push registry.example.com/safety-autonomy-core:${GITHUB_SHA}

# Deploy on target machine
docker pull registry.example.com/safety-autonomy-core:${GITHUB_SHA}
docker compose up -d
```

## Performance Optimization

### Build Optimization

```bash
# Use BuildKit for faster builds
export DOCKER_BUILDKIT=1
docker compose build

# Use layer caching
docker compose build --cache-from safety-autonomy-core:latest
```

### Production Deployment

### Building Runtime Image

For production deployment, use the optimized runtime stage:

```bash
make build-runtime
# or
docker build --target runtime -t safety-autonomy-core:runtime .
```

The runtime stage is optimized for production use:
- Smaller image size (no build tools)
- Only runtime dependencies
- Built workspace included
- Health checks enabled

### Security Scanning

Scan the Docker image for vulnerabilities before deployment:

```bash
make security-scan
# or
trivy image safety-autonomy-core:latest
```

Install Trivy if not available:
```bash
wget -qO - https://github.com/aquasecurity/trivy/releases/download/v0.50.4/trivy_0.50.4_Linux-64bit.deb
sudo dpkg -i trivy_0.50.4_Linux-64bit.deb
```

### Resource Management

The docker-compose.yml includes resource limits suitable for robotics workloads:

- **CPU Reservation**: 2 cores (guaranteed for real-time tasks)
- **CPU Limit**: 4 cores (maximum burst capacity)
- **Memory Reservation**: 4GB (guaranteed for core processes)
- **Memory Limit**: 8GB (maximum burst capacity)

These can be adjusted in docker-compose.yml based on your specific hardware requirements:

```yaml
deploy:
  resources:
    reservations:
      cpus: '4'      # Increase for more CPU-intensive workloads
      memory: 8G     # Increase for memory-intensive simulations
    limits:
      cpus: '8'      # Maximum CPU capacity
      memory: 16G    # Maximum memory capacity
```

### Health Monitoring

The container includes a health check that monitors ROS2 node availability:

```bash
# Health check configuration
interval: 30s      # Check every 30 seconds
timeout: 10s       # Fail if check takes longer than 10s
retries: 3        # Mark unhealthy after 3 consecutive failures
start_period: 5s   # Grace period on startup
```

Monitor health status:
```bash
# Check health status
docker inspect --format='{{.State.Health.Status}} safety-autonomy-demo

# View health log events
docker inspect --format='{{range .State.Health.Log}}{{.Output}}{{end}}' safety-autonomy-demo
```

### Deployment Strategies

#### Development Deployment
```bash
# Build with development stage (includes build tools)
docker compose build
# or
make build
```

#### Production Deployment
```bash
# Build with runtime stage (optimized)
make build-runtime
# or
docker build --target runtime -t safety-autonomy-core:runtime .
```

#### Registry Deployment
```bash
# Tag and push to registry
make push REGISTRY=registry.example.com TAG=v1.0.0

# Pull and run on target machine
docker pull registry.example.com/safety-autonomy-core:v1.0.0
docker run --rm registry.example.com/safety-autonomy-core:v1.0.0
```

### Continuous Integration/Deployment

The Docker setup supports CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
name: Docker Build and Deploy

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Build Docker image
        run: docker compose build
      
      - name: Run tests
        run: make test
      
      - name: Security scan
        run: make security-scan
      
      - name: Push to registry
        if: github.ref == 'refs/heads/main'
        run: make push REGISTRY=ghcr.io/${{ github.repository }} TAG=${{ github.sha }}
```

## Runtime Optimization

```bash
# Use CPU pinning for real-time performance
docker run --rm --cpuset-cpus=0-3 safety-autonomy-demo

# Use memory limits
docker run --rm --memory=4g safety-autonomy-demo

# Use shared memory for ROS2
docker run --rm --shm-size=2g safety-autonomy-demo
```

## References

- [Docker Documentation](https://docs.docker.com/)
- [ROS2 Docker Documentation](https://docs.ros.org/en/humble/Tutorials/Docker.html)
- [NVIDIA Container Toolkit](https://github.com/NVIDIA/nvidia-docker)
- [Gazebo Docker](https://gazebosim.org/docs/docker)
- [ROS2 DDS Configuration](https://docs.ros.org/en/humble/Concepts/Intermediate/About-Different-DDS-Vendors.html)

## Support

For issues specific to Docker deployment:
1. Check this guide's troubleshooting section
2. Review Docker logs: `docker compose logs`
3. Check container logs: `docker logs <container_id>`
4. Verify host system meets prerequisites
5. Open an issue on the project repository

For general Safety Autonomy Core issues, see the main README.md and documentation.
