cmake_minimum_required(VERSION 3.24)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick Network Core5Compat)

add_library(astrea-shell-runtime STATIC
    runtime/ShellRuntime.cpp
    runtime/ShellRuntime.hpp
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
    "${CMAKE_CURRENT_SOURCE_DIR}/../Dock"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Spotlight"
    "${CMAKE_CURRENT_SOURCE_DIR}/../AltTab"
)

target_link_libraries(astrea-shell-runtime PUBLIC
    astrea-alttab-core
    astrea-shared-core
    astrea-shared-typhon
    astrea-shared-layer-shell
    astrea_spotlight_backend
)

qt_add_executable(astrea-shell
    app/main.cpp
    app/AstreaShellApplication.cpp
    app/AstreaShellApplication.hpp
)
target_include_directories(astrea-shell PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
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
qt_import_qml_plugins(astrea-shell)

install(TARGETS astrea-shell RUNTIME DESTINATION bin)
install(FILES packaging/systemd/astrea-shell.service DESTINATION share/systemd/user)

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
    add_test(NAME shell-runtime-test COMMAND shell-runtime-test)

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
    add_test(NAME shell-ipc-test COMMAND shell-ipc-test)

    add_test(NAME legacy-dock-daemon-guard COMMAND $<TARGET_FILE:astrea-dock> --daemon)
    add_test(NAME legacy-alttab-daemon-guard COMMAND $<TARGET_FILE:astrea-alt-tab> --daemon)
    add_test(NAME legacy-spotlight-daemon-guard COMMAND $<TARGET_FILE:astrea-spotlight> --daemon)
    set_tests_properties(
        legacy-dock-daemon-guard legacy-alttab-daemon-guard legacy-spotlight-daemon-guard
        PROPERTIES WILL_FAIL TRUE TIMEOUT 2
    )
endif()
