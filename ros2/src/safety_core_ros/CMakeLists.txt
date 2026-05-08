cmake_minimum_required(VERSION 3.20)
project(safety_core_ros)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Release optimizations for better performance
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler optimizations
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)

  # Performance optimizations
  add_compile_options(-O3 -march=native -ffast-math)

  # Link-time optimization
  add_link_options(-flto)

  # Remove unused code
  add_compile_options(-fdata-sections -ffunction-sections)
  add_link_options(-Wl,--gc-sections)
endif()

# Set CPU affinity for better real-time performance
if(UNIX)
  add_compile_options(-pthread)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(safety_core_msgs REQUIRED)
find_package(safety_autonomy_core REQUIRED)

# The ament package "safety_autonomy_core" exports its library target as
# "safety_autonomy_core::safety_core" (auto-namespaced by ament_export_targets).
# An older standalone export named "safety_core::safety_core" also exists for
# non-ament downstream consumers but is not auto-imported here.
if(TARGET safety_autonomy_core::safety_core)
  set(SAFETY_CORE_TARGET safety_autonomy_core::safety_core)
elseif(TARGET safety_core::safety_core)
  set(SAFETY_CORE_TARGET safety_core::safety_core)
else()
  message(FATAL_ERROR
    "Cannot find safety_core library target. Expected one of: "
    "safety_autonomy_core::safety_core, safety_core::safety_core.")
endif()

# ----------------------------------------------------------------------------
# Shared bridges library: adapters between rclcpp and safety_core abstractions.
# ----------------------------------------------------------------------------
add_library(safety_core_ros_bridges
  src/ros_clock.cpp
  src/ros_diagnostic_transport.cpp
  src/ros_health_monitor.cpp
)
target_include_directories(safety_core_ros_bridges PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
# Forward include dirs explicitly because the upstream target lacks an
# INSTALL_INTERFACE include dir on its CMake-exported version.
target_include_directories(safety_core_ros_bridges SYSTEM PUBLIC
  ${safety_autonomy_core_INCLUDE_DIRS})
target_link_libraries(safety_core_ros_bridges PUBLIC ${SAFETY_CORE_TARGET})
ament_target_dependencies(safety_core_ros_bridges PUBLIC
  rclcpp safety_core_msgs builtin_interfaces std_msgs)

# ----------------------------------------------------------------------------
# Node 1: safety_envelope_node
# ----------------------------------------------------------------------------
add_executable(safety_envelope_node
  src/safety_envelope_node.cpp
  src/safety_envelope_main.cpp)
target_include_directories(safety_envelope_node PRIVATE include)
target_link_libraries(safety_envelope_node
  safety_core_ros_bridges
  ${SAFETY_CORE_TARGET})
ament_target_dependencies(safety_envelope_node
  rclcpp sensor_msgs nav_msgs geometry_msgs visualization_msgs
  std_msgs tf2 tf2_ros safety_core_msgs)

# ----------------------------------------------------------------------------
# Node 2: safety_supervisor_node (lifecycle)
# ----------------------------------------------------------------------------
add_executable(safety_supervisor_node
  src/safety_supervisor_node.cpp
  src/safety_supervisor_main.cpp)
target_include_directories(safety_supervisor_node PRIVATE include)
target_link_libraries(safety_supervisor_node
  safety_core_ros_bridges
  ${SAFETY_CORE_TARGET})
ament_target_dependencies(safety_supervisor_node
  rclcpp rclcpp_lifecycle std_msgs nav_msgs sensor_msgs
  diagnostic_msgs std_srvs safety_core_msgs)

# ----------------------------------------------------------------------------
# Node 3: safety_drive_bridge_node
# ----------------------------------------------------------------------------
add_executable(safety_drive_bridge_node
  src/safety_drive_bridge_node.cpp
  src/safety_drive_bridge_main.cpp)
target_include_directories(safety_drive_bridge_node PRIVATE include)
target_link_libraries(safety_drive_bridge_node
  safety_core_ros_bridges
  ${SAFETY_CORE_TARGET})
ament_target_dependencies(safety_drive_bridge_node
  rclcpp std_msgs geometry_msgs nav_msgs safety_core_msgs)

# ----------------------------------------------------------------------------
# Node 4: teleop_node (keyboard teleoperation for testing)
# ----------------------------------------------------------------------------
add_executable(teleop_node
  src/teleop_node.cpp)
target_include_directories(teleop_node PRIVATE include)
ament_target_dependencies(teleop_node
  rclcpp geometry_msgs std_msgs)

# ----------------------------------------------------------------------------
# Install
# ----------------------------------------------------------------------------
install(TARGETS
    safety_core_ros_bridges
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include)

install(TARGETS
    safety_envelope_node
    safety_supervisor_node
    safety_drive_bridge_node
    teleop_node
  RUNTIME DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
  DESTINATION include)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL)

ament_export_include_directories(include)
ament_export_libraries(safety_core_ros_bridges)
ament_export_dependencies(rclcpp safety_core_msgs safety_autonomy_core)
ament_package()
