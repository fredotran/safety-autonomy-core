cmake_minimum_required(VERSION 3.20)
project(safety_core_sim NONE)

find_package(ament_cmake REQUIRED)

install(DIRECTORY description config worlds models launch
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_pytest REQUIRED)

  # Structural validation: URDF/xacro/SDF well-formedness, bridge YAML parse
  ament_add_pytest_test(sim_validation test/test_sim_files.py
    TIMEOUT 60)
endif()

ament_package()
