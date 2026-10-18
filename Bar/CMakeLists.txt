cmake_minimum_required(VERSION 3.24)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)

function(bar_enable_sanitizers target)
    if(MSVC)
        return()
    endif()

    set(flags)
    if(ASTREA_ENABLE_ASAN)
        list(APPEND flags address)
    endif()
    if(ASTREA_ENABLE_UBSAN)
        list(APPEND flags undefined)
    endif()
    if(flags)
        list(JOIN flags "," sanitizer_flags)
        target_compile_options(${target} PRIVATE -fsanitize=${sanitizer_flags}
                                                  -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=${sanitizer_flags})
    endif()
endfunction()

add_library(astrea-bar-core STATIC
    core/BarController.cpp
    core/BarController.hpp
    core/BarLayoutPolicy.cpp
    core/BarLayoutPolicy.hpp
    core/BarLayoutMetrics.cpp
    core/BarLayoutMetrics.hpp
    core/BarPopupController.cpp
    core/BarPopupController.hpp
    core/WorkspaceModel.cpp
    core/WorkspaceModel.hpp
    services/BarClockService.cpp
    services/BarClockService.hpp
)
target_include_directories(astrea-bar-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(astrea-bar-core PUBLIC
    Qt6::Core
    Qt6::Gui
    astrea-shared-core
)
bar_enable_sanitizers(astrea-bar-core)

add_library(astrea-bar-wayland STATIC
    platform/wayland/BarSurfacePolicy.cpp
    platform/wayland/BarSurfacePolicy.hpp
    platform/wayland/BarSurfaceBundle.cpp
    platform/wayland/BarSurfaceBundle.hpp
    platform/wayland/BarSurfaceManager.cpp
    platform/wayland/BarSurfaceManager.hpp
)
target_include_directories(astrea-bar-wayland PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(astrea-bar-wayland PUBLIC
    astrea-bar-core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    astrea-shared-layer-shell
    PRIVATE
    astrea-shared-system-qt
    astrea-shared-statusnotifier
)
bar_enable_sanitizers(astrea-bar-wayland)

if(BUILD_TESTING AND ASTREA_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BarResources.cmake")

    add_executable(bar-core-test
        tests/BarCoreTest.cpp
    )
    target_include_directories(bar-core-test PRIVATE "${CMAKE_SOURCE_DIR}")
    target_link_libraries(bar-core-test PRIVATE
        astrea-bar-core
        Qt6::Test
    )
    bar_enable_sanitizers(bar-core-test)
    add_test(NAME bar-core-test COMMAND bar-core-test)

    add_executable(bar-wayland-test
        tests/BarWaylandTest.cpp
    )
    target_include_directories(bar-wayland-test PRIVATE "${CMAKE_SOURCE_DIR}")
    astrea_add_bar_qml_resources(bar-wayland-test)
    target_link_libraries(bar-wayland-test PRIVATE
        astrea-bar-wayland
        astrea-shared-coreplugin
        astrea-shared-system-qtplugin
        astrea-shared-wayland-effects
        astrea-shared-wayland-effectsplugin
        astrea-shared-system-qt
        astrea_system_backend
        Qt6::Test
    )
    qt_import_qml_plugins(bar-wayland-test)
    bar_enable_sanitizers(bar-wayland-test)
    add_test(NAME bar-wayland-test COMMAND bar-wayland-test)

    add_executable(bar-qml-smoke-test
        tests/BarQmlSmokeTest.cpp
    )
    target_include_directories(bar-qml-smoke-test PRIVATE "${CMAKE_SOURCE_DIR}")
    astrea_add_bar_qml_resources(bar-qml-smoke-test)
    target_link_libraries(bar-qml-smoke-test PRIVATE
        astrea-bar-core
        astrea-shared-statusnotifier
        astrea-shared-coreplugin
        astrea-shared-system-qtplugin
        astrea-shared-wayland-effects
        astrea-shared-wayland-effectsplugin
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
        astrea-shared-system-qt
        astrea_system_backend
    )
    qt_import_qml_plugins(bar-qml-smoke-test)
    bar_enable_sanitizers(bar-qml-smoke-test)
    add_test(NAME bar-qml-smoke-test COMMAND bar-qml-smoke-test)

    add_test(NAME bar-qml-legacy-guard
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bar_qml.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/qml"
            "${CMAKE_CURRENT_SOURCE_DIR}/../shared/system"
    )
    add_test(NAME bar-architecture-guard
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_bar_architecture.py"
            "${CMAKE_SOURCE_DIR}"
    )
endif()
