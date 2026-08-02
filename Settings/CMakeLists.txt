cmake_minimum_required(VERSION 3.24)
project(AstreaSettings VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ASTREA_SETTINGS_BUILD_TESTS "Build Settings unit tests" ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Core5Compat Gui Qml Quick QuickControls2)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SettingsWarnings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SettingsSanitizers.cmake)

if(NOT TARGET astrea-shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_CURRENT_BINARY_DIR}/shared)
endif()

qt_add_executable(astrea-settings
    app/main.cpp
    app/SettingsApplication.cpp
    app/SettingsApplication.hpp
    core/SettingsController.cpp
    core/SettingsController.hpp
    core/SettingsGroupMembership.cpp
    core/SettingsGroupMembership.hpp
    core/SettingsNavigationModel.cpp
    core/SettingsNavigationModel.hpp
    core/SettingsTranslationController.cpp
    core/SettingsTranslationController.hpp
    core/ThemeController.cpp
    core/ThemeController.hpp
)

target_include_directories(astrea-settings PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../shared"
)

target_link_libraries(astrea-settings PRIVATE
    astrea-shared
    astrea-sharedplugin
    Qt6::Core
    Qt6::Core5Compat
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

set(SETTINGS_QML_FILES
    qml/Main.qml
    qml/components/AppIcon.qml
    qml/components/AvatarImage.qml
    qml/components/Theme.qml
    qml/components/controls/Button.qml
    qml/components/controls/ButtonCapsule.qml
    qml/components/controls/DualButton.qml
    qml/components/controls/FloatingButton.qml
    qml/components/feedback/DnsPresetChip.qml
    qml/components/feedback/DnsStatusCard.qml
    qml/components/feedback/ProgressCard.qml
    qml/components/feedback/SpeedCard.qml
    qml/components/feedback/StatusDot.qml
    qml/components/form/FormCard.qml
    qml/components/form/IconListRow.qml
    qml/components/form/ScrollPage.qml
    qml/components/form/SearchField.qml
    qml/components/form/SectionHeader.qml
    qml/components/form/SelectButton.qml
    qml/components/form/SettingRow.qml
    qml/components/form/ToggleSwitch.qml
    qml/components/menu/ContextMenu.qml
    qml/components/menu/ContextMenuAction.qml
    qml/components/menu/ContextMenuDivider.qml
    qml/components/navigation/NavItem.qml
    qml/components/navigation/Sidebar.qml
    qml/components/navigation/SidebarFrame.qml
    qml/pages/system/Compositor.qml
    qml/components/typography/DisplayLabel.qml
    qml/components/typography/Divider.qml
    qml/components/typography/TextLabel.qml
    qml/theme/Apps.qml
    qml/theme/Shell.qml
    qml/theme/State.qml
    qml/theme/Tokens.qml
)

set(SETTINGS_ASSET_FILES
    assets/i18n/en_US.json
    assets/icons/settings/audio.svg
    assets/icons/settings/apps.svg
    assets/icons/settings/bluetooth.svg
    assets/icons/settings/display.svg
    assets/icons/settings/network.svg
    assets/icons/settings/performance.svg
    assets/icons/settings/software-center.svg
    assets/icons/settings/sound.svg
    assets/icons/settings/theme.svg
    assets/icons/settings/wallpaper.svg
    assets/icons/settings/themes/bluetooth.svg
    assets/icons/settings/themes/dark/audio.svg
    assets/icons/settings/themes/dark/apps.svg
    assets/icons/settings/themes/dark/bluetooth.svg
    assets/icons/settings/themes/dark/display.svg
    assets/icons/settings/themes/dark/network.svg
    assets/icons/settings/themes/dark/performance.svg
    assets/icons/settings/themes/dark/software-center.svg
    assets/icons/settings/themes/dark/theme.svg
    assets/icons/settings/themes/dark/wallpaper.svg
)

set_source_files_properties(
    qml/theme/Tokens.qml
    qml/theme/Apps.qml
    qml/theme/Shell.qml
    qml/theme/State.qml
    qml/components/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(astrea-settings
    URI Astrea.Settings
    VERSION 1.0
    QML_FILES ${SETTINGS_QML_FILES}
)
qt_add_resources(astrea-settings settings-assets
    PREFIX "/Astrea/Settings"
    FILES ${SETTINGS_ASSET_FILES}
)
qt_import_qml_plugins(astrea-settings)

settings_enable_warnings(astrea-settings)
settings_enable_sanitizers(astrea-settings)

install(TARGETS astrea-settings RUNTIME DESTINATION bin)
install(FILES packaging/applications/astrea-settings.desktop
    DESTINATION share/applications)

if(BUILD_TESTING AND ASTREA_SETTINGS_BUILD_TESTS AND ASTREA_BUILD_TESTS)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

    add_executable(settings-controller-test
        tests/SettingsControllerTest.cpp
        core/SettingsController.cpp
        core/SettingsController.hpp
        core/SettingsGroupMembership.cpp
        core/SettingsGroupMembership.hpp
        core/SettingsNavigationModel.cpp
        core/SettingsNavigationModel.hpp
        core/ThemeController.cpp
        core/ThemeController.hpp
    )
    target_include_directories(settings-controller-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(settings-controller-test PRIVATE
        Qt6::Core
        Qt6::Test
    )
    settings_enable_warnings(settings-controller-test)
    settings_enable_sanitizers(settings-controller-test)
    add_test(NAME settings-controller-test COMMAND settings-controller-test)

    add_executable(settings-group-membership-test
        tests/SettingsGroupMembershipTest.cpp
        core/SettingsGroupMembership.cpp
        core/SettingsGroupMembership.hpp
    )
    target_include_directories(settings-group-membership-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(settings-group-membership-test PRIVATE
        Qt6::Core
        Qt6::Test
    )
    settings_enable_warnings(settings-group-membership-test)
    settings_enable_sanitizers(settings-group-membership-test)
    add_test(NAME settings-group-membership-test COMMAND settings-group-membership-test)

    add_executable(theme-controller-test
        tests/ThemeControllerTest.cpp
        core/ThemeController.cpp
        core/ThemeController.hpp
    )
    target_include_directories(theme-controller-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(theme-controller-test PRIVATE
        Qt6::Core
        Qt6::Test
    )
    settings_enable_warnings(theme-controller-test)
    settings_enable_sanitizers(theme-controller-test)
    add_test(NAME theme-controller-test COMMAND theme-controller-test)

    qt_add_executable(settings-qml-smoke-test
        tests/SettingsQmlSmokeTest.cpp
        core/SettingsController.cpp
        core/SettingsController.hpp
        core/SettingsGroupMembership.cpp
        core/SettingsGroupMembership.hpp
        core/SettingsNavigationModel.cpp
        core/SettingsNavigationModel.hpp
        core/SettingsTranslationController.cpp
        core/SettingsTranslationController.hpp
        core/ThemeController.cpp
        core/ThemeController.hpp
    )
    target_include_directories(settings-qml-smoke-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../shared"
    )
    target_link_libraries(settings-qml-smoke-test PRIVATE
        astrea-shared
        astrea-sharedplugin
        Qt6::Core
        Qt6::Core5Compat
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Test
    )
    qt_add_qml_module(settings-qml-smoke-test
        URI Astrea.Settings
        VERSION 1.0
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Astrea/SettingsSmoke"
        QML_FILES ${SETTINGS_QML_FILES}
    )
    qt_add_resources(settings-qml-smoke-test settings-test-assets
        PREFIX "/Astrea/Settings"
        FILES ${SETTINGS_ASSET_FILES}
    )
    qt_import_qml_plugins(settings-qml-smoke-test)
    settings_enable_warnings(settings-qml-smoke-test)
    settings_enable_sanitizers(settings-qml-smoke-test)
    add_test(NAME settings-qml-smoke-test COMMAND settings-qml-smoke-test)
    set(settings_qml_smoke_environment "QT_QPA_PLATFORM=offscreen")
    if(ASTREA_ENABLE_ASAN)
        # Qt's offscreen graphics teardown leaks allocations in system font and GL libraries.
        list(APPEND settings_qml_smoke_environment "ASAN_OPTIONS=detect_leaks=0")
    endif()
    set_tests_properties(settings-qml-smoke-test PROPERTIES
        ENVIRONMENT "${settings_qml_smoke_environment}"
    )

    add_test(NAME compositor-page-source-test
        COMMAND ${CMAKE_COMMAND}
            -DCOMPOSITOR_QML_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/qml/pages/system/Compositor.qml
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/CompositorPageSourceTest.cmake
    )
endif()
