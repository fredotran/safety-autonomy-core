cmake_minimum_required(VERSION 3.20)
project(safety_core_msgs)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(geometry_msgs REQUIRED)

set(msg_files
  msg/SafetyZone.msg
  msg/SafetyState.msg
  msg/EnvelopeStatus.msg
  msg/DiagnosticEvent.msg
  msg/MonitorEvent.msg
)

rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  DEPENDENCIES std_msgs builtin_interfaces geometry_msgs
)

ament_export_dependencies(rosidl_default_runtime std_msgs builtin_interfaces geometry_msgs)
ament_package()
