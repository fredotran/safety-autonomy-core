cmake_minimum_required(VERSION 3.20)
project(safety_core_test)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_gtest REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(safety_core_msgs REQUIRED)
find_package(safety_core_ros REQUIRED)

# Test Fixtures Library (shared helpers)
add_library(test_fixtures INTERFACE)
target_include_directories(test_fixtures INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/fixtures>
  $<INSTALL_INTERFACE:include>)

# Unit Tests: time_utils
ament_add_gtest(test_time_utils
  test/unit/test_time_utils.cpp)
target_include_directories(test_time_utils PRIVATE
  ${CMAKE_SOURCE_DIR}/../safety_core_ros/include)
target_link_libraries(test_time_utils test_fixtures)
ament_target_dependencies(test_time_utils rclcpp)

# Unit Tests: math_utils
ament_add_gtest(test_math_utils
  test/unit/test_math_utils.cpp)
target_include_directories(test_math_utils PRIVATE
  ${CMAKE_SOURCE_DIR}/../safety_core_ros/include)
target_link_libraries(test_math_utils test_fixtures)
ament_target_dependencies(test_math_utils rclcpp)

# Component Tests: sensor_monitor_node
ament_add_gtest(test_sensor_monitor
  test/component/test_sensor_monitor.cpp)
target_include_directories(test_sensor_monitor PRIVATE
  ${CMAKE_SOURCE_DIR}/../safety_core_ros/include)
target_link_libraries(test_sensor_monitor safety_core_ros::safety_core_ros_bridges)
ament_target_dependencies(test_sensor_monitor
  rclcpp sensor_msgs nav_msgs safety_core_msgs safety_core_ros)

ament_package()
