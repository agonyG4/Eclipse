function(astrea_add_theme_consumer_qml_test)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick Test)
    add_executable(astrea-theme-consumers-qml-test
        "${CMAKE_SOURCE_DIR}/shared/tests/ThemeConsumersQmlTest.cpp")
    set_target_properties(astrea-theme-consumers-qml-test PROPERTIES AUTOMOC ON)
    target_include_directories(astrea-theme-consumers-qml-test PRIVATE
        "${CMAKE_SOURCE_DIR}/shared"
        "${CMAKE_SOURCE_DIR}/Dock")
    target_compile_definitions(astrea-theme-consumers-qml-test PRIVATE
        ASTREA_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}"
        ASTREA_BAR_THEME_QML="${CMAKE_SOURCE_DIR}/Bar/qml/components/ShellBarTheme.qml"
        ASTREA_MENU_THEME_QML="${CMAKE_SOURCE_DIR}/Shell/qml/contextmenu/ShellMenuTheme.qml"
        ASTREA_DOCK_PANEL_QML="${CMAKE_SOURCE_DIR}/Dock/qml/components/DockPanel.qml"
        ASTREA_ALTTAB_DELEGATE_QML="${CMAKE_SOURCE_DIR}/AltTab/qml/components/AltTabWindowDelegate.qml"
        ASTREA_ALTTAB_PANEL_QML="${CMAKE_SOURCE_DIR}/AltTab/qml/components/AltTabPanel.qml"
        ASTREA_SPOTLIGHT_WEATHER_QML="${CMAKE_SOURCE_DIR}/Spotlight/qml/components/SpotlightWeatherChip.qml"
        ASTREA_SPOTLIGHT_PANEL_QML="${CMAKE_SOURCE_DIR}/Spotlight/qml/components/SpotlightPanel.qml")
    target_link_libraries(astrea-theme-consumers-qml-test PRIVATE
        astrea-shared-core astrea-shared-coreplugin
        astrea-shared-wayland-effects astrea-shared-wayland-effectsplugin
        astrea-settings-ui astrea-settings-uiplugin
        astrea-dock-runtime
        "$<LINK_GROUP:RESCAN,astrea-shared-system-qt,${CMAKE_BINARY_DIR}/shared/libastrea_system_backend.a>"
        Qt6::Test Qt6::Qml Qt6::Quick)
    astrea_shared_enable_sanitizers(astrea-theme-consumers-qml-test)
    add_test(NAME astrea-theme-consumers-qml-test COMMAND astrea-theme-consumers-qml-test)
    set_tests_properties(astrea-theme-consumers-qml-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${CMAKE_BINARY_DIR}")
endfunction()
