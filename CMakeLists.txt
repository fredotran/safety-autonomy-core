cmake_minimum_required(VERSION 3.20)

project(safety_autonomy_core VERSION 0.1.0 LANGUAGES CXX)

option(SAFETY_CORE_ENABLE_SANITIZERS "Enable runtime sanitizers for non-safety builds" ON)
option(SAFETY_CORE_ENABLE_WERROR "Treat compiler warnings as errors" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(safety_core STATIC
    src/result.cpp
    src/common/time.cpp
    src/diagnostics/health_state.cpp
    src/state_machine/state_machine.cpp
    src/exec/task_executor.cpp
    src/platform/clock.cpp
    src/diag/logging_health_monitor.cpp
    src/control/safety_pid.cpp
    src/filters/alpha_beta.cpp
)

target_include_directories(safety_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

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

install(TARGETS safety_core
    EXPORT safety_core_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)

install(DIRECTORY include/ DESTINATION include)

export(EXPORT safety_core_targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/safety_coreTargets.cmake"
    NAMESPACE safety_core::)
