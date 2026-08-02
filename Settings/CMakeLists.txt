cmake_minimum_required(VERSION 3.24)
project(AstreaSettings VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ASTREA_SETTINGS_BUILD_TESTS "Build Settings unit tests" ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

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
)

target_include_directories(astrea-settings PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../shared"
)

target_link_libraries(astrea-settings PRIVATE
    astrea-shared
    astrea-sharedplugin
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

set_source_files_properties(
    qml/theme/Tokens.qml
    qml/theme/Palette.qml
    qml/theme/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(astrea-settings
    URI Astrea.Settings
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/components/AppShell.qml
        qml/components/EmptyContent.qml
        qml/components/ProfileAvatar.qml
        qml/components/ProfileHeader.qml
        qml/components/SettingsSidebar.qml
        qml/components/SidebarCollapseButton.qml
        qml/components/SidebarFrame.qml
        qml/components/SidebarItem.qml
        qml/components/WindowTitleBar.qml
        qml/theme/Palette.qml
        qml/theme/Theme.qml
        qml/theme/Tokens.qml
        qml/ui/DisplayLabel.qml
        qml/ui/Divider.qml
        qml/ui/FormCard.qml
        qml/ui/PrimaryButton.qml
        qml/ui/ScrollPage.qml
        qml/ui/SearchField.qml
        qml/ui/SectionHeader.qml
        qml/ui/SettingRow.qml
        qml/ui/TextLabel.qml
        qml/ui/ToggleSwitch.qml
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
