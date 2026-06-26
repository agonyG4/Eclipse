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
find_package(LayerShellQt QUIET)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/RustBackend.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SpotlightWarnings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SpotlightSanitizers.cmake)

# Link shared libraries sibling folder
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_CURRENT_BINARY_DIR}/shared)

astrea_add_rust_backend(astrea_spotlight_rust_backend
    MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/backend/Cargo.toml"
    TARGET_DIR "${CMAKE_BINARY_DIR}/cargo-target"
    HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/backend/include"
)

qt_add_executable(astrea-spotlight
    app/main.cpp
    app/CommandLine.cpp
    app/SpotlightApplication.cpp
    core/SpotlightController.cpp
    core/SpotlightResultsModel.cpp
    services/ApplicationLauncher.cpp
    services/GameModeMonitor.cpp
    services/SpotlightConfigWatcher.cpp
    services/AstreaI18n.cpp
    platform/ipc/SpotlightIpcServer.cpp
    platform/wayland/LayerShellSurface.cpp
    platform/runtime/SpotlightRuntimePaths.cpp
    platform/rust/RustSpotlightBackend.cpp
)

target_include_directories(astrea-spotlight PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/backend/include"
)

target_link_libraries(astrea-spotlight PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network
    astrea_spotlight_backend
    astrea-shared
    dl pthread
)

if(LayerShellQt_FOUND)
    target_link_libraries(astrea-spotlight PRIVATE LayerShellQt::Interface)
    target_compile_definitions(astrea-spotlight PRIVATE ASTREA_HAVE_LAYER_SHELL_QT=1)
else()
    target_compile_definitions(astrea-spotlight PRIVATE ASTREA_HAVE_LAYER_SHELL_QT=0)
endif()

qt_add_qml_module(astrea-spotlight
    URI Astrea.Spotlight
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/components/SpotlightPanel.qml
        qml/components/SpotlightResultsList.qml
        qml/components/SpotlightWeatherChip.qml
        qml/components/AstreaAppIcon.qml
)

astrea_enable_warnings(astrea-spotlight)
astrea_enable_sanitizers(astrea-spotlight)

install(TARGETS astrea-spotlight RUNTIME DESTINATION bin)
install(FILES packaging/systemd/astrea-spotlightd.service DESTINATION share/systemd/user)

include(CTest)
if(BUILD_TESTING AND ASTREA_SPOTLIGHT_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

    add_executable(spotlight-tests
        tests/SpotlightBackendTest.cpp
        core/SpotlightController.cpp
        core/SpotlightResultsModel.cpp
        services/ApplicationLauncher.cpp
        services/GameModeMonitor.cpp
        services/SpotlightConfigWatcher.cpp
        services/AstreaI18n.cpp
        platform/ipc/SpotlightIpcServer.cpp
        platform/wayland/LayerShellSurface.cpp
        platform/runtime/SpotlightRuntimePaths.cpp
        platform/rust/RustSpotlightBackend.cpp
    )
    target_include_directories(spotlight-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/backend/include"
    )
    target_link_libraries(spotlight-tests PRIVATE
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network Qt6::Test
        astrea_spotlight_backend
        astrea-shared
        dl pthread
    )
    if(LayerShellQt_FOUND)
        target_link_libraries(spotlight-tests PRIVATE LayerShellQt::Interface)
    endif()
    add_test(NAME spotlight-tests COMMAND spotlight-tests)
endif()
