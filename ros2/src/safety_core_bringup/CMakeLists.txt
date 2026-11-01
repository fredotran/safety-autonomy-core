cmake_minimum_required(VERSION 3.20)
project(safety_core_bringup NONE)

find_package(ament_cmake REQUIRED)

install(DIRECTORY launch config rviz maps scripts
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_pytest REQUIRED)

  # Structural validation: launch files compile/load, YAML configs parse,
  # RViz configs are valid XML. Catches breakage before demo launch.
  ament_add_pytest_test(launch_validation test/test_launch_files.py
    TIMEOUT 60)
endif()

ament_package()
