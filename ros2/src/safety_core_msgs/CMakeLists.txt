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

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  # Guards the constant values that mirror safety_core enums on the wire
  rosidl_get_typesupport_target(cpp_typesupport_target
    ${PROJECT_NAME} "rosidl_typesupport_cpp")
  ament_add_gtest(test_msg_interface test/test_msg_interface.cpp)
  target_link_libraries(test_msg_interface ${cpp_typesupport_target})
endif()

ament_package()
