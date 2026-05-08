cmake_minimum_required(VERSION 3.20)

project(safety_autonomy_core VERSION 0.1.0 LANGUAGES CXX)

option(SAFETY_CORE_ENABLE_SANITIZERS "Enable runtime sanitizers for non-safety builds" ON)
option(SAFETY_CORE_ENABLE_WERROR "Treat compiler warnings as errors" ON)
option(SAFETY_CORE_ENABLE_COVERAGE "Enable coverage instrumentation" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CMakePackageConfigHelpers)

option(SAFETY_CORE_ENABLE_AMENT "Enable ROS 2 ament package integration" OFF)

if(SAFETY_CORE_ENABLE_AMENT)
    find_package(ament_cmake REQUIRED)
endif()

add_library(safety_core STATIC
    src/result.cpp
    src/common/time.cpp
    src/diagnostics/health_state.cpp
    src/state_machine/state_machine.cpp
    src/exec/task_executor.cpp
    src/platform/clock.cpp
    src/diag/logging_health_monitor.cpp
    src/diag/logging_diagnostic_transport.cpp
    src/control/safety_pid.cpp
    src/filters/alpha_beta.cpp
    src/filters/complementary_filter.cpp
    src/filters/bounded_ekf_filter.cpp
    src/motion/trajectory.cpp
    src/safety/safety_supervisor.cpp
    src/config/validation.cpp
    src/config/migration.cpp
    src/config/env_loader.cpp
    src/system/context_factory.cpp
)

target_include_directories(safety_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

add_executable(safety_core_app
    src/app/safety_core_app.cpp
)

target_link_libraries(safety_core_app PRIVATE safety_core)

target_compile_options(safety_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Wdouble-promotion
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Woverloaded-virtual
    -Wundef
)

if(SAFETY_CORE_ENABLE_WERROR)
    target_compile_options(safety_core PRIVATE -Werror)
endif()

if(SAFETY_CORE_ENABLE_SANITIZERS)
    target_compile_options(safety_core PRIVATE -fsanitize=address,undefined)
    # Link options must propagate so downstream executables are linked with sanitizers.
    target_link_options(safety_core PUBLIC -fsanitize=address,undefined)
endif()

if(SAFETY_CORE_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(safety_core PRIVATE --coverage -O0 -g)
        target_link_options(safety_core PUBLIC --coverage)
    else()
        message(WARNING "Coverage requested but compiler does not support --coverage")
    endif()
endif()

enable_testing()

add_executable(safety_core_smoke
    tests/state_machine_tests.cpp
)

target_link_libraries(safety_core_smoke PRIVATE safety_core)

add_test(NAME safety_core_smoke COMMAND safety_core_smoke)

add_executable(safety_envelope_smoke
    tests/safety_envelope_tests.cpp
)

target_link_libraries(safety_envelope_smoke PRIVATE safety_core)

add_test(NAME safety_envelope_smoke COMMAND safety_envelope_smoke)

add_executable(system_context_smoke
    tests/system_context_tests.cpp
)

target_link_libraries(system_context_smoke PRIVATE safety_core)

add_test(NAME system_context_smoke COMMAND system_context_smoke)

add_executable(pid_controller_tests
    tests/pid_controller_tests.cpp
)

target_link_libraries(pid_controller_tests PRIVATE safety_core)

add_test(NAME pid_controller_tests COMMAND pid_controller_tests)

add_executable(executor_watchdog_tests
    tests/executor_watchdog_tests.cpp
)

target_link_libraries(executor_watchdog_tests PRIVATE safety_core)

add_test(NAME executor_watchdog_tests COMMAND executor_watchdog_tests)

add_executable(context_factory_tests
    tests/context_factory_tests.cpp
)

target_link_libraries(context_factory_tests PRIVATE safety_core)

add_test(NAME context_factory_tests COMMAND context_factory_tests)

add_executable(env_loader_tests
    tests/env_loader_tests.cpp
)

target_link_libraries(env_loader_tests PRIVATE safety_core)

add_test(NAME env_loader_tests COMMAND env_loader_tests)

add_executable(diagnostic_truncation_tests
    tests/diagnostic_truncation_tests.cpp
)

target_link_libraries(diagnostic_truncation_tests PRIVATE safety_core)

add_test(NAME diagnostic_truncation_tests COMMAND diagnostic_truncation_tests)

add_executable(no_allocation_policy_tests
    tests/no_allocation_policy_tests.cpp
)

target_link_libraries(no_allocation_policy_tests PRIVATE safety_core)

add_test(NAME no_allocation_policy_tests COMMAND no_allocation_policy_tests)

add_executable(deterministic_replay_tests
    tests/deterministic_replay_tests.cpp
)

target_link_libraries(deterministic_replay_tests PRIVATE safety_core)

add_test(NAME deterministic_replay_tests COMMAND deterministic_replay_tests)

add_executable(filter_invariants_tests
    tests/filter_invariants_tests.cpp
)

target_link_libraries(filter_invariants_tests PRIVATE safety_core)

add_test(NAME filter_invariants_tests COMMAND filter_invariants_tests)

add_executable(motion_trajectory_tests
    tests/motion_trajectory_tests.cpp
)

target_link_libraries(motion_trajectory_tests PRIVATE safety_core)

add_test(NAME motion_trajectory_tests COMMAND motion_trajectory_tests)

add_executable(health_beacon_tests
    tests/health_beacon_tests.cpp
)

target_link_libraries(health_beacon_tests PRIVATE safety_core)

add_test(NAME health_beacon_tests COMMAND health_beacon_tests)

add_executable(fault_injection_tests
    tests/fault_injection_tests.cpp
)

target_link_libraries(fault_injection_tests PRIVATE safety_core)

add_test(NAME fault_injection_tests COMMAND fault_injection_tests)

add_executable(safety_supervisor_tests
    tests/safety_supervisor_tests.cpp
)

target_link_libraries(safety_supervisor_tests PRIVATE safety_core)

add_test(NAME safety_supervisor_tests COMMAND safety_supervisor_tests)

add_executable(production_improvements_tests
    tests/production_improvements_tests.cpp
)

target_link_libraries(production_improvements_tests PRIVATE safety_core)

add_test(NAME production_improvements_tests COMMAND production_improvements_tests)

add_executable(non_regression_tests
    tests/non_regression_tests.cpp
)

target_link_libraries(non_regression_tests PRIVATE safety_core)

add_test(NAME non_regression_tests COMMAND non_regression_tests)

add_executable(agv_safety_demo
    examples/agv_safety_demo.cpp
)

target_link_libraries(agv_safety_demo PRIVATE safety_core)

# Commentary: Package smoke test is defined inline to guarantee install/export + find_package validity.
set(SAFETY_CORE_PACKAGE_SMOKE_ROOT "${CMAKE_BINARY_DIR}/package_smoke")
set(SAFETY_CORE_PACKAGE_SMOKE_INSTALL_DIR "${SAFETY_CORE_PACKAGE_SMOKE_ROOT}/install")
set(SAFETY_CORE_PACKAGE_SMOKE_CONSUMER_DIR "${SAFETY_CORE_PACKAGE_SMOKE_ROOT}/consumer")

file(MAKE_DIRECTORY "${SAFETY_CORE_PACKAGE_SMOKE_CONSUMER_DIR}")
file(WRITE "${SAFETY_CORE_PACKAGE_SMOKE_CONSUMER_DIR}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.20)
project(safety_core_consumer_smoke LANGUAGES CXX)
find_package(safety_core REQUIRED CONFIG)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE safety_core::safety_core)
]=])
file(WRITE "${SAFETY_CORE_PACKAGE_SMOKE_CONSUMER_DIR}/main.cpp" [=[
#include "safety_core/result.hpp"

int main() {
  return safety_core::Result::Ok().ok() ? 0 : 1;
}
]=])

add_test(NAME package_install_smoke
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${SAFETY_CORE_PACKAGE_SMOKE_INSTALL_DIR})

add_test(NAME package_consumer_configure_smoke
    COMMAND ${CMAKE_COMMAND}
        -S ${SAFETY_CORE_PACKAGE_SMOKE_CONSUMER_DIR}
        -B ${SAFETY_CORE_PACKAGE_SMOKE_CONSUMER_DIR}/build
        -DCMAKE_PREFIX_PATH=${SAFETY_CORE_PACKAGE_SMOKE_INSTALL_DIR})
set_tests_properties(package_consumer_configure_smoke PROPERTIES DEPENDS package_install_smoke)

add_test(NAME package_config_smoke
    COMMAND ${CMAKE_COMMAND} --build ${SAFETY_CORE_PACKAGE_SMOKE_CONSUMER_DIR}/build)
set_tests_properties(package_config_smoke PROPERTIES DEPENDS package_consumer_configure_smoke)

set(SAFETY_CORE_CONFIG_INSTALL_DIR "lib/cmake/safety_core")

# Commentary: Generate package config inline to keep packaging logic centralized in this CMakeLists.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/safety_coreConfig.cmake
[=[include("${CMAKE_CURRENT_LIST_DIR}/safety_coreTargets.cmake")
]=])

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/safety_coreConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(TARGETS safety_core
    EXPORT safety_core_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)

install(TARGETS safety_core_app
    RUNTIME DESTINATION bin)

install(DIRECTORY include/ DESTINATION include)

install(EXPORT safety_core_targets
    FILE safety_coreTargets.cmake
    NAMESPACE safety_core::
    DESTINATION ${SAFETY_CORE_CONFIG_INSTALL_DIR})

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/safety_coreConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/safety_coreConfigVersion.cmake
    DESTINATION ${SAFETY_CORE_CONFIG_INSTALL_DIR})

export(EXPORT safety_core_targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/safety_coreTargets.cmake"
    NAMESPACE safety_core::)

# Packaging configuration
set(CPACK_PACKAGE_NAME "safety-core")
set(CPACK_PACKAGE_VENDOR "LeafHatRobotics Inc.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Safety Autonomy Core library and demo application")
set(CPACK_PACKAGE_CONTACT "support@leafhatrobotics.com")
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/build-coverage/;/.git/;.gitlab-ci.yml" )
include(CPack)

if(SAFETY_CORE_ENABLE_AMENT)
    install(EXPORT safety_core_targets
        FILE safety_coreTargets.cmake
        NAMESPACE safety_core::
        DESTINATION share/${PROJECT_NAME}/cmake)
    ament_export_targets(safety_core_targets HAS_LIBRARY_TARGET)
    ament_export_include_directories(include)
    ament_package()
endif()
