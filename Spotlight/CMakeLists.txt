cmake_minimum_required(VERSION 3.24)
project(AstreaSpotlight VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ASTREA_SPOTLIGHT_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick Network)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/RustBackend.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SpotlightWarnings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SpotlightSanitizers.cmake)

# Link shared libraries sibling folder
if(NOT TARGET astrea-shared-core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_CURRENT_BINARY_DIR}/shared)
endif()

astrea_add_rust_backend(astrea_spotlight_rust_backend
    MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/backend/Cargo.toml"
    TARGET_DIR "${CMAKE_BINARY_DIR}/cargo-target"
    HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/backend/include"
)

add_library(astrea-spotlight-runtime STATIC
    core/SpotlightBackend.hpp
    core/SpotlightControllerPaths.hpp
    core/SpotlightController.cpp
    core/SpotlightController.hpp
    core/SpotlightResultsModel.cpp
    core/SpotlightResultsModel.hpp
    services/AstreaI18n.cpp
    services/AstreaI18n.hpp
    services/GameModeMonitor.cpp
    services/GameModeMonitor.hpp
    services/SpotlightConfigWatcher.cpp
    services/SpotlightConfigWatcher.hpp
    platform/runtime/SpotlightRuntimePaths.cpp
    platform/runtime/SpotlightRuntimePaths.hpp
    platform/rust/RustSpotlightBackend.cpp
    platform/rust/RustSpotlightBackend.hpp
)
target_include_directories(astrea-spotlight-runtime PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(astrea-spotlight-runtime
    PUBLIC
        Qt6::Core
        astrea-shared-core
    PRIVATE
        astrea_spotlight_backend
        dl
        pthread
)
astrea_enable_warnings(astrea-spotlight-runtime)
astrea_enable_sanitizers(astrea-spotlight-runtime)

add_library(astrea-spotlight-wayland STATIC
    platform/wayland/LayerShellSurface.cpp
    platform/wayland/LayerShellSurface.hpp
)
target_include_directories(astrea-spotlight-wayland PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(astrea-spotlight-wayland
    PUBLIC Qt6::Core
    PRIVATE Qt6::Gui Qt6::Quick astrea-shared-layer-shell
)
astrea_enable_warnings(astrea-spotlight-wayland)
astrea_enable_sanitizers(astrea-spotlight-wayland)

qt_add_executable(astrea-spotlight
    app/main.cpp
    app/CommandLine.cpp
    ../Shell/platform/ipc/ShellIpcClient.cpp
)

target_include_directories(astrea-spotlight PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Shell"
)

target_link_libraries(astrea-spotlight PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network
    astrea-shared-core
)

astrea_enable_warnings(astrea-spotlight)
astrea_enable_sanitizers(astrea-spotlight)

install(TARGETS astrea-spotlight RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING AND ASTREA_SPOTLIGHT_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

    add_executable(spotlight-tests
        tests/SpotlightBackendTest.cpp
        platform/ipc/SpotlightIpcServer.cpp
    )
    target_include_directories(spotlight-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(spotlight-tests PRIVATE
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network Qt6::Test
        astrea-spotlight-runtime
        astrea-shared-core
    )
    astrea_enable_sanitizers(spotlight-tests)
    add_test(NAME spotlight-tests COMMAND spotlight-tests)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(
        NAME spotlight-architecture-guard
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_spotlight_architecture.py"
    )
    add_test(
        NAME layer-shell-activation-structure
        COMMAND ${CMAKE_COMMAND}
            -DSPOTLIGHT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/static/LayerShellActivationStructureTest.cmake
    )
    if(ASTREA_ENABLE_ASAN)
        set_tests_properties(spotlight-tests PROPERTIES
            ENVIRONMENT
                "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/AltTab/lsan.supp:print_suppressions=0;ASAN_OPTIONS=detect_leaks=1:halt_on_error=1;UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
        )
    endif()
endif()
