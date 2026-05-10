# Multi-stage Dockerfile for safety-autonomy-core ROS2 demo
# Stage 1: Base image with build dependencies (cached separately)
FROM ros:jazzy-perception AS base

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install build dependencies, ROS2 packages, Python deps, and Xvfb in a single layer
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    gcc \
    g++ \
    make \
    ninja-build \
    python3-pip \
    python3-vcstool \
    python3-colcon-common-extensions \
    python3-rosdep \
    ros-jazzy-nav2-bringup \
    ros-jazzy-nav2-lifecycle-manager \
    ros-jazzy-nav2-collision-monitor \
    ros-jazzy-slam-toolbox \
    ros-jazzy-rviz2 \
    ros-jazzy-robot-localization \
    ros-jazzy-joint-state-publisher \
    ros-jazzy-xacro \
    ros-jazzy-behaviortree-cpp-v3 \
    ros-jazzy-nav2-behavior-tree \
    ros-jazzy-ros-gz-sim \
    ros-jazzy-ros-gz-bridge \
    xvfb \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --break-system-packages --no-cache-dir \
    pytest \
    pytest-cov \
    vcstool

# Stage 2: Development stage with workspace setup and build
FROM base AS development

# Set working directory
WORKDIR /workspace

# Copy repository files
COPY . /workspace/safety-autonomy-core

# Set up ROS2 workspace structure
WORKDIR /workspace/ros2_ws

# Create src directory first
RUN mkdir -p /workspace/ros2_ws/src

# Copy the ros2 source directory contents to the workspace src (excluding symlinks)
RUN cd /workspace/safety-autonomy-core/ros2/src && \
    for dir in */; do \
        if [ -L "$dir" ]; then \
            echo "Skipping symlink: $dir"; \
        else \
            cp -r "$dir" /workspace/ros2_ws/src/; \
        fi \
    done

# Install dependencies using rosdep (skip already installed packages)
RUN bash -c "source /opt/ros/jazzy/setup.bash && \
    cd /workspace/ros2_ws && \
    mkdir -p /home/rosdep_cache && \
    rosdep update && \
    rosdep install --from-paths src --ignore-src -r -y \
        --skip-keys='ros-jazzy-ros-gz-sim ros-jazzy-ros-gz-bridge ros-jazzy-joint-state-publisher ros-jazzy-xacro ros-jazzy-nav2-bringup ros-jazzy-nav2-lifecycle-manager ros-jazzy-nav2-collision-monitor ros-jazzy-slam-toolbox ros-jazzy-rviz2 ros-jazzy-robot-localization ros-jazzy-behaviortree-cpp-v3 ros-jazzy-nav2-behavior-tree' || true"

# Build the workspace with safety-critical flags and parallel workers
RUN bash -c "source /opt/ros/jazzy/setup.bash && \
    cd /workspace/ros2_ws && \
    colcon build \
        --cmake-args -DSAFETY_CORE_ENABLE_AMENT=ON \
                     -DSAFETY_CORE_ENABLE_SANITIZERS=OFF \
                     -DSAFETY_CORE_ENABLE_WERROR=OFF \
        --cmake-args --parallel-workers $(nproc) \
        --event-handlers console_direct+"

# Set up environment
RUN echo ". /opt/ros/jazzy/setup.bash" >> /root/.bashrc && \
    echo ". /workspace/ros2_ws/install/setup.bash" >> /root/.bashrc

# Create entry point script for display forwarding and initialization
RUN echo '#!/bin/bash\n\
# Display forwarding setup\n\
if [ -n "$DISPLAY" ]; then\n\
    export DISPLAY=$DISPLAY\n\
fi\n\
# Source ROS2 environment\n\
. /opt/ros/jazzy/setup.bash\n\
. /workspace/ros2_ws/install/setup.bash\n\
\n\
# Execute the command\n\
exec "$@"' > /usr/local/bin/docker-entrypoint.sh && \
    chmod +x /usr/local/bin/docker-entrypoint.sh

# Expose ROS2 ports
EXPOSE 11311

# Set entry point
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]

# Default command
CMD ["/bin/bash"]

# Stage 3: Runtime stage (optimized for production)
FROM base AS runtime

# Set working directory
WORKDIR /workspace

# Copy only the built workspace from development stage
COPY --from=development /workspace/ros2_ws /workspace/ros2_ws

# Set up environment
RUN echo ". /opt/ros/jazzy/setup.bash" >> /root/.bashrc && \
    echo ". /workspace/ros2_ws/install/setup.bash" >> /root/.bashrc

# Create entry point script
RUN echo '#!/bin/bash\n\
# Display forwarding setup\n\
if [ -n "$DISPLAY" ]; then\n\
    export DISPLAY=$DISPLAY\n\
fi\n\
# Source ROS2 environment\n\
. /opt/ros/jazzy/setup.bash\n\
. /workspace/ros2_ws/install/setup.bash\n\
\n\
# Execute the command\n\
exec "$@"' > /usr/local/bin/docker-entrypoint.sh && \
    chmod +x /usr/local/bin/docker-entrypoint.sh

# Expose ROS2 ports
EXPOSE 11311

# Health check for ROS2 nodes
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD source /opt/ros/jazzy/setup.bash && \
        source /workspace/ros2_ws/install/setup.bash && \
        ros2 node list || exit 1

# Set entry point
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]

# Default command
CMD ["/bin/bash"]
