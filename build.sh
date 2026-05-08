#!/usr/bin/env bash
#
# build.sh - Comprehensive build script for safety-autonomy-core
#
# This script provides a unified interface for building the project with various options:
# - Local CMake build (core library)
# - ROS 2 colcon build (ROS 2 packages)
# - Docker-based build (containerized)
#
# Usage: ./build.sh [OPTIONS]
#
# Options:
#   -h, --help              Show this help message
#   -t, --type TYPE         Build type: cmake, ros2, docker (default: cmake)
#   -m, --mode MODE         Build mode: Debug, Release, RelWithDebInfo (default: RelWithDebInfo)
#   -s, --sanitizers        Enable address/undefined sanitizers (CMake builds only)
#   -c, --clean             Clean build directory before building
#   -j, --jobs N            Number of parallel jobs (default: auto-detect)
#   -v, --verbose           Enable verbose output
#   --test                 Run tests after build
#   --no-skip              Don't skip ROS 2 packages (build all)
#

set -euo pipefail

# Default values
BUILD_TYPE="cmake"
BUILD_MODE="RelWithDebInfo"
ENABLE_SANITIZERS=false
CLEAN_BUILD=false
PARALLEL_JOBS=""
VERBOSE=false
RUN_TESTS=false
SKIP_PACKAGES="safety_core_nav2 safety_core_bringup"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Print usage
print_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Build script for safety-autonomy-core

Options:
  -h, --help              Show this help message
  -t, --type TYPE         Build type: cmake, ros2, docker, all (default: cmake)
  -m, --mode MODE         Build mode: Debug, Release, RelWithDebInfo (default: RelWithDebInfo)
  -s, --sanitizers        Enable address/undefined sanitizers (CMake builds only)
  -c, --clean             Clean build directory before building
  -j, --jobs N            Number of parallel jobs (default: auto-detect)
  -v, --verbose           Enable verbose output
  --test                 Run tests after build
  --no-skip              Don't skip ROS 2 packages (build all)

Examples:
  $0                                    # Build core library with CMake (RelWithDebInfo)
  $0 -t cmake -m Debug -s              # Build core library with Debug mode and sanitizers
  $0 -t ros2 --test                    # Build ROS 2 packages and run tests
  $0 -t docker -c                      # Clean build with Docker
  $0 -t cmake -j 8                     # Build with 8 parallel jobs
  $0 -t all --test                     # Build both CMake and ROS 2 packages

EOF
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                print_usage
                exit 0
                ;;
            -t|--type)
                BUILD_TYPE="$2"
                shift 2
                ;;
            -m|--mode)
                BUILD_MODE="$2"
                shift 2
                ;;
            -s|--sanitizers)
                ENABLE_SANITIZERS=true
                shift
                ;;
            -c|--clean)
                CLEAN_BUILD=true
                shift
                ;;
            -j|--jobs)
                PARALLEL_JOBS="$2"
                shift 2
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            --test)
                RUN_TESTS=true
                shift
                ;;
            --no-skip)
                SKIP_PACKAGES=""
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                print_usage
                exit 1
                ;;
        esac
    done

    # Validate BUILD_TYPE
    if [[ ! "$BUILD_TYPE" =~ ^(cmake|ros2|docker|all)$ ]]; then
        log_error "Invalid build type: $BUILD_TYPE. Must be cmake, ros2, docker, or all"
        exit 1
    fi

    # Validate BUILD_MODE
    if [[ ! "$BUILD_MODE" =~ ^(Debug|Release|RelWithDebInfo)$ ]]; then
        log_error "Invalid build mode: $BUILD_MODE. Must be Debug, Release, or RelWithDebInfo"
        exit 1
    fi
}

# Detect number of CPU cores for parallel builds
detect_parallel_jobs() {
    if [[ -z "$PARALLEL_JOBS" ]]; then
        if command -v nproc &> /dev/null; then
            PARALLEL_JOBS=$(nproc)
        elif command -v sysctl &> /dev/null; then
            PARALLEL_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
        else
            PARALLEL_JOBS=4
        fi
    fi
    log_info "Using $PARALLEL_JOBS parallel jobs"
}

# Clean build directory
clean_build_dir() {
    local build_dir="$1"
    if [[ "$CLEAN_BUILD" == true ]]; then
        log_info "Cleaning build directory: $build_dir"
        rm -rf "$build_dir"
    fi
}

# Build with CMake (core library)
build_cmake() {
    log_info "Building core library with CMake"
    log_info "Build mode: $BUILD_MODE"
    log_info "Sanitizers: $ENABLE_SANITIZERS"

    local build_dir="build"
    clean_build_dir "$build_dir"

    local cmake_args=(
        -DCMAKE_BUILD_TYPE="$BUILD_MODE"
    )

    if [[ "$ENABLE_SANITIZERS" == true ]]; then
        cmake_args+=(-DSAFETY_CORE_ENABLE_SANITIZERS=ON)
    fi

    if [[ "$VERBOSE" == true ]]; then
        cmake_args+=(-DCMAKE_VERBOSE_MAKEFILE=ON)
    fi

    log_info "Configuring with CMake..."
    /usr/bin/cmake -S . -B "$build_dir" "${cmake_args[@]}"

    log_info "Building with make..."
    local build_args=()
    if [[ -n "$PARALLEL_JOBS" ]]; then
        build_args+=(-j "$PARALLEL_JOBS")
    fi
    if [[ "$VERBOSE" == true ]]; then
        build_args+=(VERBOSE=1)
    fi

    make -C "$build_dir" "${build_args[@]}"

    if [[ "$RUN_TESTS" == true ]]; then
        log_info "Running tests with CTest..."
        /usr/bin/ctest --test-dir "$build_dir" --output-on-failure
    fi

    log_success "Build completed successfully"
}

# Build all (CMake + ROS 2)
build_all() {
    log_info "Building both CMake core library and ROS 2 packages"

    # First build CMake core library
    log_info "Step 1: Building CMake core library..."
    build_cmake

    # Then build ROS 2 packages
    log_info "Step 2: Building ROS 2 packages..."
    build_ros2

    log_success "All builds completed successfully"
}

# Build with ROS 2 colcon
build_ros2() {
    log_info "Building ROS 2 packages with colcon"
    log_info "Build mode: $BUILD_MODE"

    # Check if ROS 2 environment is sourced
    if [[ -z "${ROS_DISTRO:-}" ]]; then
        log_warning "ROS 2 environment not found. Attempting to source ROS 2..."
        
        # Try to find and source ROS 2 setup
        if [[ -f "/opt/ros/jazzy/setup.bash" ]]; then
            log_info "Sourcing ROS 2 Jazzy environment..."
            source /opt/ros/jazzy/setup.bash
        elif [[ -f "/opt/ros/humble/setup.bash" ]]; then
            log_info "Sourcing ROS 2 Humble environment..."
            source /opt/ros/humble/setup.bash
        elif [[ -f "/opt/ros/iron/setup.bash" ]]; then
            log_info "Sourcing ROS 2 Iron environment..."
            source /opt/ros/iron/setup.bash
        else
            log_error "ROS 2 not found. Please install ROS 2 or source the setup.bash manually"
            log_error "Example: source /opt/ros/\$ROS_DISTRO/setup.bash"
            exit 1
        fi
    fi

    log_info "ROS_DISTRO: $ROS_DISTRO"

    # Check if colcon is available
    if ! command -v colcon &> /dev/null; then
        log_error "colcon not found. Please install colcon:"
        log_error "  sudo apt install python3-colcon-common-extensions"
        exit 1
    fi

    local build_dir="ros2/build"
    clean_build_dir "$build_dir"

    local colcon_args=(
        --base-paths . ros2/src
        --cmake-args -DCMAKE_BUILD_TYPE="$BUILD_MODE"
        -DSAFETY_CORE_ENABLE_AMENT=ON
        -DSAFETY_CORE_ENABLE_SANITIZERS=OFF
        -DSAFETY_CORE_ENABLE_WERROR=OFF
    )

    if [[ -n "$SKIP_PACKAGES" ]]; then
        colcon_args+=(--packages-skip $SKIP_PACKAGES)
        log_info "Skipping packages: $SKIP_PACKAGES"
    fi

    if [[ -n "$PARALLEL_JOBS" ]]; then
        colcon_args+=(--parallel-workers "$PARALLEL_JOBS")
    fi

    if [[ "$VERBOSE" == true ]]; then
        colcon_args+=(--event-handlers console_direct+)
    fi

    log_info "Building with colcon..."
    colcon build "${colcon_args[@]}"

    if [[ "$RUN_TESTS" == true ]]; then
        log_info "Running tests with colcon..."
        colcon test --packages-select safety_core_ros --event-handlers console_direct+
        colcon test-result --all
    fi

    log_success "ROS 2 colcon build completed successfully"
}

# Build with Docker
build_docker() {
    log_info "Building with Docker"
    log_info "Build mode: $BUILD_MODE"

    # Check if Docker is available
    if ! command -v docker &> /dev/null; then
        log_error "Docker not found. Please install Docker first"
        exit 1
    fi

    if [[ "$CLEAN_BUILD" == true ]]; then
        log_info "Cleaning Docker resources..."
        docker compose down -v || true
        docker system prune -f || true
    fi

    log_info "Building Docker image with docker compose..."
    docker compose build

    log_success "Docker build completed successfully"
    log_info "To run the demo, use: make demo-full"
}

# Main function
main() {
    log_info "Starting build process for safety-autonomy-core"
    log_info "Build type: $BUILD_TYPE"

    parse_args "$@"
    detect_parallel_jobs

    case "$BUILD_TYPE" in
        cmake)
            build_cmake
            ;;
        ros2)
            build_ros2
            ;;
        docker)
            build_docker
            ;;
        all)
            build_all
            ;;
    esac

    log_success "Build process completed successfully!"
}

# Run main function
main "$@"
