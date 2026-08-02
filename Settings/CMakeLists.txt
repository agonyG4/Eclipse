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
    QML_FILES
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
        qml/components/typography/DisplayLabel.qml
        qml/components/typography/Divider.qml
        qml/components/typography/TextLabel.qml
        qml/theme/Apps.qml
        qml/theme/Shell.qml
        qml/theme/State.qml
        qml/theme/Tokens.qml
)
qt_add_resources(astrea-settings settings-assets
    PREFIX "/Astrea/Settings"
    FILES
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
endif()
