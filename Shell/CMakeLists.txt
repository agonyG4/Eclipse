cmake_minimum_required(VERSION 3.24)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick Network Core5Compat)

function(shell_enable_sanitizers target)
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

add_library(astrea-shell-runtime STATIC
    ../Bar/core/BarClockService.cpp
    ../Bar/core/BarClockService.hpp
    ../Bar/core/BarController.cpp
    ../Bar/core/BarController.hpp
    ../Bar/core/BarLayoutMetrics.cpp
    ../Bar/core/BarLayoutMetrics.hpp
    ../Bar/core/BarPopupController.cpp
    ../Bar/core/BarPopupController.hpp
    ../Bar/core/BarSurfacePolicy.cpp
    ../Bar/core/BarSurfacePolicy.hpp
    ../Bar/core/WorkspaceModel.cpp
    ../Bar/core/WorkspaceModel.hpp
    runtime/ShellRuntime.cpp
    runtime/ShellRuntime.hpp
    platform/shortcut/ShellShortcutDispatcher.cpp
    platform/shortcut/ShellShortcutDispatcher.hpp
    platform/ipc/ShellIpcServer.cpp
    platform/ipc/ShellIpcServer.hpp
    ../Dock/core/DockAppInfo.cpp
    ../Dock/core/DockAppModel.cpp
    ../Dock/core/DockController.cpp
    ../Dock/platform/runtime/DockRuntimePaths.cpp
    ../Dock/platform/wayland/DockLayerShellSurface.cpp
    ../Dock/services/DockConfigWatcher.cpp
    ../Spotlight/core/SpotlightController.cpp
    ../Spotlight/core/SpotlightResultsModel.cpp
    ../Spotlight/platform/runtime/SpotlightRuntimePaths.cpp
    ../Spotlight/platform/wayland/LayerShellSurface.cpp
    ../Spotlight/platform/rust/RustSpotlightBackend.cpp
    ../Spotlight/services/AstreaI18n.cpp
    ../Spotlight/services/GameModeMonitor.cpp
    ../Spotlight/services/SpotlightConfigWatcher.cpp
)

target_include_directories(astrea-shell-runtime PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../Bar"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Dock"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight"
    "${CMAKE_CURRENT_SOURCE_DIR}/../AltTab"
)

target_link_libraries(astrea-shell-runtime PUBLIC
    astrea-alttab-core
    astrea-shared-core
    astrea-shared-statusnotifier
    astrea-shared-system
    astrea-shared-typhon
    astrea-shared-layer-shell
    astrea_spotlight_backend
)
shell_enable_sanitizers(astrea-shell-runtime)

qt_add_executable(astrea-shell
    app/main.cpp
    app/AstreaShellApplication.cpp
    app/AstreaShellApplication.hpp
    ../Bar/platform/wayland/BarSurfaceBundle.cpp
    ../Bar/platform/wayland/BarSurfaceBundle.hpp
    ../Bar/platform/wayland/BarSurfaceManager.cpp
    ../Bar/platform/wayland/BarSurfaceManager.hpp
)
target_include_directories(astrea-shell PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../Bar"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Dock"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight"
    "${CMAKE_CURRENT_SOURCE_DIR}/../AltTab"
)
target_link_libraries(astrea-shell PRIVATE
    astrea-shell-runtime
    astrea-shared-coreplugin
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network Qt6::Core5Compat
    dl pthread
)
shell_enable_sanitizers(astrea-shell)

include("${CMAKE_CURRENT_SOURCE_DIR}/../Bar/cmake/BarResources.cmake")

set(_shell_qml_files
    "${CMAKE_CURRENT_SOURCE_DIR}/../Dock/qml/Main.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Dock/qml/components/DockPanel.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Dock/qml/components/DockAppDelegate.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../AltTab/qml/Main.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../AltTab/qml/components/AltTabPanel.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../AltTab/qml/components/AltTabWindowDelegate.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../AltTab/qml/components/AstreaAppIcon.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight/qml/Main.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight/qml/components/SpotlightPanel.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight/qml/components/SpotlightResultsList.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight/qml/components/SpotlightWeatherChip.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight/qml/components/AstreaAppIcon.qml"
)
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../Dock/qml/Main.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Dock/qml/Main.qml")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../Dock/qml/components/DockPanel.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Dock/qml/components/DockPanel.qml")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../Dock/qml/components/DockAppDelegate.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Dock/qml/components/DockAppDelegate.qml")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../AltTab/qml/Main.qml"
    PROPERTIES QT_RESOURCE_ALIAS "AltTab/qml/Main.qml")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../AltTab/qml/components/AltTabPanel.qml"
    PROPERTIES QT_RESOURCE_ALIAS "AltTab/qml/components/AltTabPanel.qml")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../AltTab/qml/components/AltTabWindowDelegate.qml"
    PROPERTIES QT_RESOURCE_ALIAS "AltTab/qml/components/AltTabWindowDelegate.qml")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../AltTab/qml/components/AstreaAppIcon.qml"
    PROPERTIES QT_RESOURCE_ALIAS "AltTab/qml/components/AstreaAppIcon.qml")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight/qml/Main.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Spotlight/qml/Main.qml")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight/qml/components/SpotlightPanel.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Spotlight/qml/components/SpotlightPanel.qml")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight/qml/components/SpotlightResultsList.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Spotlight/qml/components/SpotlightResultsList.qml")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight/qml/components/SpotlightWeatherChip.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Spotlight/qml/components/SpotlightWeatherChip.qml")
set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight/qml/components/AstreaAppIcon.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Spotlight/qml/components/AstreaAppIcon.qml")
qt_add_qml_module(astrea-shell
    URI Astrea.Shell
    VERSION 1.0
    QML_FILES ${_shell_qml_files}
)
astrea_add_bar_qml_resources(astrea-shell)
qt_import_qml_plugins(astrea-shell)

install(TARGETS astrea-shell RUNTIME DESTINATION bin)
install(FILES packaging/systemd/astrea-shell.service DESTINATION share/systemd/user)
install(PROGRAMS tools/astrea-migrate-shell-systemd DESTINATION bin)

if(BUILD_TESTING AND ASTREA_BUILD_TESTS)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)
    add_executable(shell-runtime-test
        tests/ShellRuntimeTest.cpp
    )
    target_include_directories(shell-runtime-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_link_libraries(shell-runtime-test PRIVATE
        astrea-shell-runtime
        Qt6::Test
    )
    shell_enable_sanitizers(shell-runtime-test)
    add_test(NAME shell-runtime-test COMMAND shell-runtime-test)

    add_executable(bar-core-test
        ../Bar/tests/BarCoreTest.cpp
        ../Bar/platform/wayland/BarSurfaceBundle.cpp
        ../Bar/platform/wayland/BarSurfaceBundle.hpp
        ../Bar/platform/wayland/BarSurfaceManager.cpp
        ../Bar/platform/wayland/BarSurfaceManager.hpp
    )
    target_include_directories(bar-core-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        "${CMAKE_CURRENT_SOURCE_DIR}/../Bar"
    )
    target_link_libraries(bar-core-test PRIVATE
        astrea-shell-runtime
        Qt6::Qml Qt6::Quick
        Qt6::Test
    )
    shell_enable_sanitizers(bar-core-test)
    add_test(NAME bar-core-test COMMAND bar-core-test)

    add_executable(bar-qml-smoke-test
        ../Bar/tests/BarQmlSmokeTest.cpp
    )
    astrea_add_bar_qml_resources(bar-qml-smoke-test)
    target_link_libraries(bar-qml-smoke-test PRIVATE
        astrea-shell-runtime
        Qt6::Qml Qt6::Quick Qt6::Test
    )
    shell_enable_sanitizers(bar-qml-smoke-test)
    add_test(NAME bar-qml-smoke-test COMMAND bar-qml-smoke-test)

    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME bar-qml-legacy-guard
             COMMAND ${Python3_EXECUTABLE}
                     "${CMAKE_CURRENT_SOURCE_DIR}/../Bar/tests/check_bar_qml.py"
                     "${CMAKE_CURRENT_SOURCE_DIR}/../Bar/qml"
                     "${CMAKE_CURRENT_SOURCE_DIR}/../shared/system")

    add_executable(shell-ipc-test
        tests/ShellIpcTest.cpp
    )
    target_include_directories(shell-ipc-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_link_libraries(shell-ipc-test PRIVATE
        astrea-shell-runtime
        Qt6::Test
    )
    shell_enable_sanitizers(shell-ipc-test)
    add_test(NAME shell-ipc-test COMMAND shell-ipc-test)

    add_executable(shell-shortcut-dispatcher-test
        tests/ShellShortcutDispatcherTest.cpp
    )
    target_include_directories(shell-shortcut-dispatcher-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_sources(shell-shortcut-dispatcher-test PRIVATE
        ../AltTab/platform/compositor/FakeBackend.cpp
    )
    target_link_libraries(shell-shortcut-dispatcher-test PRIVATE
        astrea-shell-runtime
        Qt6::Test
    )
    shell_enable_sanitizers(shell-shortcut-dispatcher-test)
    add_test(NAME shell-shortcut-dispatcher-test COMMAND shell-shortcut-dispatcher-test)

    if(ASTREA_TYPHON_WAYLAND_AVAILABLE AND ASTREA_WAYLAND_SERVER_FOUND)
        add_executable(shell-unified-runtime-integration-test
            tests/ShellUnifiedRuntimeIntegrationTest.cpp
            "${CMAKE_BINARY_DIR}/shared/generated/typhon/astrea-toplevel-management-v1-server-protocol.c"
            "${CMAKE_BINARY_DIR}/shared/generated/typhon/astrea-shortcuts-v1-server-protocol.c"
            "${CMAKE_BINARY_DIR}/shared/generated/typhon/astrea-shell-auth-v1-server-protocol.c"
        )
        target_include_directories(shell-unified-runtime-integration-test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/.."
            "${CMAKE_CURRENT_BINARY_DIR}/../shared/generated/typhon"
        )
        target_link_libraries(shell-unified-runtime-integration-test PRIVATE
            astrea-shell-runtime
            Qt6::Test
            PkgConfig::ASTREA_WAYLAND_SERVER
        )
        shell_enable_sanitizers(shell-unified-runtime-integration-test)
        add_test(NAME shell-unified-runtime-integration-test
                 COMMAND shell-unified-runtime-integration-test)
    else()
        add_test(NAME shell-unified-runtime-integration-test
            COMMAND ${CMAKE_COMMAND} -E echo
                    "SKIPPED: Wayland client/server development files are unavailable")
        set_tests_properties(shell-unified-runtime-integration-test PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIPPED")
    endif()

    add_test(NAME shell-systemd-migration-test
             COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tools/test-systemd-migration.sh")
    add_test(NAME shell-resource-measurement-test
             COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tools/test-resource-measurement.sh")

    add_test(NAME legacy-dock-daemon-guard COMMAND $<TARGET_FILE:astrea-dock> --daemon)
    add_test(NAME legacy-alttab-daemon-guard COMMAND $<TARGET_FILE:astrea-alt-tab> --daemon)
    add_test(NAME legacy-spotlight-daemon-guard COMMAND $<TARGET_FILE:astrea-spotlight> --daemon)
    set_tests_properties(
        legacy-dock-daemon-guard legacy-alttab-daemon-guard legacy-spotlight-daemon-guard
        PROPERTIES WILL_FAIL TRUE TIMEOUT 2
    )

    if(ASTREA_ENABLE_ASAN)
        set_tests_properties(
            legacy-dock-daemon-guard legacy-alttab-daemon-guard legacy-spotlight-daemon-guard
            PROPERTIES TIMEOUT 10
        )
    endif()

    if(ASTREA_ENABLE_ASAN)
        set_tests_properties(
            shell-runtime-test shell-ipc-test shell-shortcut-dispatcher-test
            shell-unified-runtime-integration-test
            PROPERTIES
            ENVIRONMENT
                "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/AltTab/lsan.supp:print_suppressions=0;ASAN_OPTIONS=detect_leaks=1:halt_on_error=1;UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
        )
    endif()
endif()
