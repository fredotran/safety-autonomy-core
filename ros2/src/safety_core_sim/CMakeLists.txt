cmake_minimum_required(VERSION 3.20)
project(safety_core_sim NONE)

find_package(ament_cmake REQUIRED)

install(DIRECTORY description config worlds models launch
  DESTINATION share/${PROJECT_NAME})

ament_package()
