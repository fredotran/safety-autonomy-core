cmake_minimum_required(VERSION 3.20)
project(safety_core_bringup NONE)

find_package(ament_cmake REQUIRED)

install(DIRECTORY launch config rviz scripts
  DESTINATION share/${PROJECT_NAME})

ament_package()
