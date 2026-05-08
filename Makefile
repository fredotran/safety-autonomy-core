# Makefile for Docker-based development and deployment
# Provides convenient shortcuts for common Docker operations

.PHONY: help build rebuild shell demo-full demo-sim demo-safety demo-sim-only clean logs test push build-runtime security-scan

# Default target
help:
	@echo "Safety Autonomy Core Docker Makefile"
	@echo "Docker is the default deployment mode (no local ROS2 required)"
	@echo ""
	@echo "Available targets:"
	@echo "  make build              - Build the Docker image (development stage)"
	@echo "  make build-runtime      - Build the Docker image (runtime stage, production)"
	@echo "  make rebuild            - Rebuild the Docker image (no cache)"
	@echo "  make shell              - Launch interactive bash shell in container"
	@echo "  make demo-full          - Run full demo (Gazebo + Safety + Nav2 + RViz)"
	@echo "  make demo-sim           - Run safety stack with simulation"
	@echo "  make demo-safety        - Run safety stack only (no simulation)"
	@echo "  make demo-sim-only      - Run simulation only (no safety stack)"
	@echo "  make clean              - Remove Docker containers and images"
	@echo "  make logs               - Show container logs"
	@echo "  make test               - Run tests in container"
	@echo "  make push               - Push image to registry (set REGISTRY and TAG)"
	@echo "  make security-scan      - Scan Docker image for vulnerabilities"
	@echo ""
	@echo "Quick start:"
	@echo "  ./setup_demo.sh         - Run demo with Docker (default)"
	@echo "  ./setup_demo.sh --local - Run demo with local ROS2 installation"
	@echo ""
	@echo "Environment variables:"
	@echo "  REGISTRY               - Docker registry (e.g., registry.example.com)"
	@echo "  TAG                    - Image tag (default: latest)"
	@echo "  DISPLAY                - Display variable for X11 forwarding (default: :0)"

# Default values
REGISTRY ?= 
TAG ?= latest
DISPLAY ?= :0

# Build the development stage Docker image
build:
	docker compose build

# Build the runtime stage Docker image (production)
build-runtime:
	docker build --target runtime -t safety-autonomy-core:runtime .

# Rebuild the Docker image (no cache)
rebuild:
	docker compose build --no-cache

# Launch interactive bash shell
shell:
	docker compose run --rm safety-autonomy-demo

# Run full demo
demo-full:
	docker compose run --rm safety-autonomy-demo \
		bash -c "source install/setup.bash && ros2 launch safety_core_bringup agv_warehouse.launch.py"

# Run safety stack with simulation
demo-sim:
	docker compose run --rm safety-autonomy-demo \
		bash -c "source install/setup.bash && ros2 launch safety_core_bringup safety_sim.launch.py"

# Run safety stack only
demo-safety:
	docker compose run --rm safety-autonomy-demo \
		bash -c "source install/setup.bash && ros2 launch safety_core_bringup safety_only.launch.py"

# Run simulation only
demo-sim-only:
	docker compose run --rm safety-autonomy-demo \
		bash -c "source install/setup.bash && ros2 launch safety_core_sim sim_only.launch.py"

# Clean up Docker resources
clean:
	docker compose down -v
	docker system prune -f

# Show container logs
logs:
	docker compose logs -f

# Run tests
test:
	docker compose run --rm safety-autonomy-demo \
		bash -c "cd /workspace/ros2_ws && colcon test --event-handlers console_direct+"

# Push image to registry
push:
	@if [ -z "$(REGISTRY)" ]; then \
		echo "Error: REGISTRY environment variable not set"; \
		echo "Usage: make push REGISTRY=registry.example.com TAG=v1.0.0"; \
		exit 1; \
	fi
	docker tag safety-autonomy-core:latest $(REGISTRY)/safety-autonomy-core:$(TAG)
	docker push $(REGISTRY)/safety-autonomy-core:$(TAG)

# Security scan (requires Trivy)
security-scan:
	@command -v trivy >/dev/null 2>&1 || { echo "Error: trivy not installed. Install from https://github.com/aquasecurity/trivy"; exit 1; }
	trivy image safety-autonomy-core:latest

# Development helpers
dev-build:
	docker compose run --rm safety-autonomy-demo \
		bash -c "cd /workspace/ros2_ws && colcon build --cmake-args -DSAFETY_CORE_ENABLE_AMENT=ON"

dev-shell:
	docker compose run --rm safety-autonomy-demo bash

# GPU-enabled targets
gpu-demo-full:
	docker compose run --rm --gpus all safety-autonomy-demo \
		bash -c "source install/setup.bash && ros2 launch safety_core_bringup agv_warehouse.launch.py"
