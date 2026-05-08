cmake_minimum_required(VERSION 3.20)
project(safety_core_nav2)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(behaviortree_cpp_v3 REQUIRED)
find_package(nav2_behavior_tree REQUIRED)
find_package(safety_core_msgs REQUIRED)

# ----------------------------------------------------------------------------
# BT plugin: IsSafeCondition
# Returns SUCCESS if the latest /safety/state is not in SafeStop and not
# fault-latched, FAILURE otherwise. Used inside Nav2's behavior tree to gate
# motion attempts and to react to safety supervisor escalation.
# ----------------------------------------------------------------------------
add_library(is_safe_condition_bt_node SHARED
  src/is_safe_condition.cpp
)
target_include_directories(is_safe_condition_bt_node PRIVATE include)
ament_target_dependencies(is_safe_condition_bt_node
  rclcpp
  std_msgs
  behaviortree_cpp_v3
  nav2_behavior_tree
  safety_core_msgs)
target_compile_definitions(is_safe_condition_bt_node PRIVATE BT_PLUGIN_EXPORT)

install(TARGETS is_safe_condition_bt_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(DIRECTORY include/
  DESTINATION include)

install(FILES safety_core_nav2_bt_plugins.xml
  DESTINATION share/${PROJECT_NAME})

ament_export_include_directories(include)
ament_export_libraries(is_safe_condition_bt_node)
ament_export_dependencies(rclcpp safety_core_msgs nav2_behavior_tree)
ament_package()
