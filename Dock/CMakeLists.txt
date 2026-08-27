cmake_minimum_required(VERSION 3.24)
project(AstreaDock VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ASTREA_DOCK_BUILD_TESTS "Build Dock unit tests" ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick Network Core5Compat)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DockWarnings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DockSanitizers.cmake)

qt_add_executable(astrea-dock
    app/main.cpp
    app/CommandLine.cpp
    ../Shell/platform/ipc/ShellIpcClient.cpp
)
target_include_directories(astrea-dock PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Shell"
)
target_link_libraries(astrea-dock PRIVATE
    Qt6::Core Qt6::Network
)

dock_enable_warnings(astrea-dock)
dock_enable_sanitizers(astrea-dock)

install(TARGETS astrea-dock RUNTIME DESTINATION bin)

if(BUILD_TESTING AND ASTREA_DOCK_BUILD_TESTS AND ASTREA_BUILD_TESTS)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)
    add_executable(dock-app-model-test
        tests/DockAppModelTest.cpp
        core/DockAppModel.cpp
    )
    target_include_directories(dock-app-model-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../shared")
    target_link_libraries(dock-app-model-test PRIVATE astrea-shared-core Qt6::Core Qt6::Gui Qt6::Test)
    dock_enable_sanitizers(dock-app-model-test)
    add_test(NAME dock-app-model-test COMMAND dock-app-model-test)

    add_executable(dock-config-watcher-test
        tests/DockConfigWatcherTest.cpp
        services/DockConfigWatcher.cpp
        services/DockConfigValidation.cpp
    )
    target_include_directories(dock-config-watcher-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../shared")
    target_link_libraries(dock-config-watcher-test PRIVATE astrea-shared-core Qt6::Core Qt6::Gui Qt6::Test)
    dock_enable_sanitizers(dock-config-watcher-test)
    add_test(NAME dock-config-watcher-test COMMAND dock-config-watcher-test)

    add_executable(dock-config-persistence-test
        tests/DockConfigPersistenceTest.cpp
        services/DockConfigPersistence.cpp
        services/DockConfigValidation.cpp
        services/DockConfigWatcher.cpp
    )
    target_include_directories(dock-config-persistence-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../shared")
    target_link_libraries(dock-config-persistence-test PRIVATE
        astrea-shared-core Qt6::Core Qt6::Gui Qt6::Test)
    dock_enable_sanitizers(dock-config-persistence-test)
    add_test(NAME dock-config-persistence-test COMMAND dock-config-persistence-test)

    add_executable(dock-runtime-paths-test
        tests/DockRuntimePathsTest.cpp
        platform/runtime/DockRuntimePaths.cpp
    )
    target_include_directories(dock-runtime-paths-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../shared")
    target_link_libraries(dock-runtime-paths-test PRIVATE Qt6::Core Qt6::Test)
    dock_enable_sanitizers(dock-runtime-paths-test)
    add_test(NAME dock-runtime-paths-test COMMAND dock-runtime-paths-test)

    add_executable(dock-controller-test
        tests/DockControllerTest.cpp
        core/DockController.cpp
        core/DockAppModel.cpp
        services/DockConfigPersistence.cpp
        services/DockConfigValidation.cpp
        services/DockConfigWatcher.cpp
    )
    target_include_directories(dock-controller-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../shared")
    target_link_libraries(dock-controller-test PRIVATE
        astrea-shared-core astrea-shared-typhon Qt6::Core Qt6::Gui Qt6::Test)
    dock_enable_sanitizers(dock-controller-test)
    add_test(NAME dock-controller-test COMMAND dock-controller-test)

    add_executable(dock-typhon-runtime-integration-test
        tests/DockTyphonRuntimeIntegrationTest.cpp
        core/DockController.cpp
        core/DockAppModel.cpp
        services/DockConfigPersistence.cpp
        services/DockConfigValidation.cpp
        services/DockConfigWatcher.cpp
    )
    target_include_directories(dock-typhon-runtime-integration-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../shared")
    target_link_libraries(dock-typhon-runtime-integration-test PRIVATE
        astrea-shared-core astrea-shared-typhon Qt6::Core Qt6::Gui Qt6::Test)
    dock_enable_sanitizers(dock-typhon-runtime-integration-test)
    add_test(NAME dock-typhon-runtime-integration-test
             COMMAND dock-typhon-runtime-integration-test)

    add_executable(dock-ipc-test
        tests/DockIpcTest.cpp
        platform/ipc/DockIpcServer.cpp
    )
    target_include_directories(dock-ipc-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(dock-ipc-test PRIVATE Qt6::Core Qt6::Network Qt6::Test)
    dock_enable_sanitizers(dock-ipc-test)
    add_test(NAME dock-ipc-test COMMAND dock-ipc-test)

    add_executable(dock-command-line-test
        tests/CommandLineTest.cpp
        app/CommandLine.cpp
    )
    target_include_directories(dock-command-line-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(dock-command-line-test PRIVATE Qt6::Core Qt6::Test)
    dock_enable_sanitizers(dock-command-line-test)
    add_test(NAME dock-command-line-test COMMAND dock-command-line-test)

    add_executable(dock-layer-shell-surface-test
        tests/DockLayerShellSurfaceTest.cpp
        platform/wayland/DockLayerShellSurface.cpp
    )
    target_include_directories(dock-layer-shell-surface-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../shared")
    target_link_libraries(dock-layer-shell-surface-test PRIVATE
        astrea-shared-layer-shell Qt6::Core Qt6::Gui Qt6::Quick Qt6::Test)
    dock_enable_sanitizers(dock-layer-shell-surface-test)
    add_test(NAME dock-layer-shell-surface-test COMMAND dock-layer-shell-surface-test)

    add_executable(dock-hover-qml-test
        tests/DockHoverQmlTest.cpp
        core/DockController.cpp
        core/DockAppModel.cpp
        core/DockSurfaceGeometry.cpp
        services/DockConfigPersistence.cpp
        services/DockConfigValidation.cpp
        services/DockConfigWatcher.cpp
    )
    target_compile_definitions(dock-hover-qml-test PRIVATE
        DOCK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        DOCK_BUILD_DIR="${CMAKE_BINARY_DIR}")
    target_include_directories(dock-hover-qml-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../shared")
    target_link_libraries(dock-hover-qml-test PRIVATE
        astrea-shared-core astrea-shared-coreplugin astrea-shared-typhon
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
    dock_enable_sanitizers(dock-hover-qml-test)
    add_test(NAME dock-hover-qml-test COMMAND dock-hover-qml-test)

    if(ASTREA_ENABLE_ASAN)
        set_tests_properties(
            dock-app-model-test dock-config-watcher-test dock-runtime-paths-test
            dock-config-persistence-test dock-controller-test dock-typhon-runtime-integration-test
            dock-layer-shell-surface-test dock-hover-qml-test
            dock-ipc-test dock-command-line-test
            PROPERTIES ENVIRONMENT
                "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/AltTab/lsan.supp:print_suppressions=0;ASAN_OPTIONS=detect_leaks=1:halt_on_error=1;UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
        )
    endif()
endif()
