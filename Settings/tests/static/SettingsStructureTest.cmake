if(NOT DEFINED SETTINGS_SOURCE_DIR)
    message(FATAL_ERROR "SETTINGS_SOURCE_DIR is required")
endif()
if(NOT DEFINED SETTINGS_QML_FILES)
    message(FATAL_ERROR "SETTINGS_QML_FILES is required")
endif()

string(REPLACE "|" ";" registered_qml_files "${SETTINGS_QML_FILES}")

set(settings_desktop_file "${SETTINGS_SOURCE_DIR}/packaging/applications/astrea-settings.desktop")
if(NOT EXISTS "${settings_desktop_file}")
    message(FATAL_ERROR "Settings desktop entry is missing: ${settings_desktop_file}")
endif()
file(READ "${settings_desktop_file}" settings_desktop_source)
string(FIND "${settings_desktop_source}" "Exec=astrea-settings\n" settings_exec_position)
if(settings_exec_position EQUAL -1)
    message(FATAL_ERROR "Settings desktop entry must launch astrea-settings")
endif()

foreach(relative_path IN LISTS registered_qml_files)
    if(NOT EXISTS "${SETTINGS_SOURCE_DIR}/qml/${relative_path}")
        message(FATAL_ERROR "Registered QML file is missing: ${relative_path}")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/pages/appearance/Wallpaper.qml" wallpaper_source)
foreach(wallpaper_forbidden_token IN ITEMS
    "Quickshell"
    "Quickshell.Io"
    "Process {"
    "python3"
    "zenity"
    "wallpaper_manager.py"
    "ASTREA_ROOT"
    "XDG_DATA_HOME"
    "XDG_CONFIG_HOME"
    "Hyprland"
    "Typhon"
)
    string(FIND "${wallpaper_source}" "${wallpaper_forbidden_token}" wallpaper_token_position)
    if(NOT wallpaper_token_position EQUAL -1)
        message(FATAL_ERROR "Forbidden token '${wallpaper_forbidden_token}' found in Wallpaper.qml")
    endif()
endforeach()

foreach(wallpaper_required_token IN ITEMS
    "visible: root.controller.errorMessage !== \"\""
    "closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside"
    "Keys.onReturnPressed"
    "Keys.onEnterPressed"
    "Keys.onEscapePressed"
    "e.g. Tokyo Night"
    "e.g. Mountain Sunset"
    "maximumLength: 128"
    "text: I18n.tr(\"apps.settings.pages.paper.wallpaper.text.confirm\", \"Confirm\")"
    "root.clearPendingWallpaperState()"
)
    string(FIND "${wallpaper_source}" "${wallpaper_required_token}" wallpaper_required_position)
    if(wallpaper_required_position EQUAL -1)
        message(FATAL_ERROR "Wallpaper.qml is missing required closure contract '${wallpaper_required_token}'")
    endif()
endforeach()

foreach(wallpaper_closure_forbidden_token IN ITEMS
    "operation_in_progress"
    "text: I18n.tr(\"apps.settings.pages.paper.wallpaper.text.add\", \"Add\")"
    "enabled: wallpaperNameInput.text.trim() !== \"\""
)
    string(FIND "${wallpaper_source}" "${wallpaper_closure_forbidden_token}" wallpaper_forbidden_closure_position)
    if(NOT wallpaper_forbidden_closure_position EQUAL -1)
        message(FATAL_ERROR "Wallpaper.qml retains forbidden closure behavior '${wallpaper_closure_forbidden_token}'")
    endif()
endforeach()

set(deleted_legacy_paths
    qml/components/AppShell.qml
    qml/components/EmptyContent.qml
    qml/components/ProfileAvatar.qml
    qml/components/ProfileHeader.qml
    qml/components/SettingsSidebar.qml
    qml/components/SidebarCollapseButton.qml
    qml/components/SidebarItem.qml
    qml/components/WindowTitleBar.qml
    qml/theme/Palette.qml
    qml/theme/Theme.qml
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
foreach(relative_path IN LISTS deleted_legacy_paths)
    list(FIND registered_qml_files "${relative_path}" registered_index)
    if(NOT registered_index EQUAL -1)
        message(FATAL_ERROR "Deleted legacy-redesign QML file is registered: ${relative_path}")
    endif()
endforeach()

set(deleted_migration_paths
    core/SettingsNavigationModel.cpp
    core/SettingsNavigationModel.hpp
    core/SettingsTranslationController.cpp
    core/SettingsTranslationController.hpp
    core/ThemeController.cpp
    core/ThemeController.hpp
    core/SettingsGroupMembership.cpp
    core/SettingsGroupMembership.hpp
    tests/SettingsControllerTest.cpp
    tests/SettingsGroupMembershipTest.cpp
    tests/SettingsQmlSmokeTest.cpp
    tests/ThemeControllerTest.cpp
    tests/CompositorPageSourceTest.cmake
)
foreach(relative_path IN LISTS deleted_migration_paths)
    if(EXISTS "${SETTINGS_SOURCE_DIR}/${relative_path}")
        message(FATAL_ERROR "Deleted migration-era path has reappeared: ${relative_path}")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/CMakeLists.txt" settings_root_cmake)
file(READ "${SETTINGS_SOURCE_DIR}/core/CMakeLists.txt" settings_core_cmake)
file(READ "${SETTINGS_SOURCE_DIR}/app/SettingsApplication.cpp" settings_application_source)
file(READ "${SETTINGS_SOURCE_DIR}/tests/CMakeLists.txt" settings_tests_cmake)
if(settings_tests_cmake MATCHES "qt_add_qml_module")
    message(FATAL_ERROR "Settings tests must consume astrea-settings-ui, not declare a QML module")
endif()

if(settings_core_cmake MATCHES "astrea-shared-core")
    message(FATAL_ERROR "astrea-settings-core must not link astrea-shared-core")
endif()
foreach(core_boundary_token IN ITEMS
    "target_link_libraries(astrea-settings-core PUBLIC"
    "Qt6::Core"
    "astrea-shared-dock"
    "target_link_libraries(astrea-settings-core PRIVATE"
    "Qt6::Network"
    "../../shared"
)
    string(FIND "${settings_core_cmake}" "${core_boundary_token}" core_boundary_position)
    if(core_boundary_position EQUAL -1)
        message(FATAL_ERROR "Settings core dependency boundary is missing '${core_boundary_token}'")
    endif()
endforeach()
if(settings_core_cmake MATCHES "target_link_libraries\\(astrea-settings-core PUBLIC[^)]*Qt6::Network")
    message(FATAL_ERROR "Qt6::Network must remain private to astrea-settings-core")
endif()

foreach(redundant_context_property IN ITEMS
    "setContextProperty(QStringLiteral(\"WallpaperController\")"
    "setContextProperty(QStringLiteral(\"AstreaIconProvider\")"
)
    string(FIND "${settings_application_source}" "${redundant_context_property}" context_property_position)
    if(NOT context_property_position EQUAL -1)
        message(FATAL_ERROR "Redundant Settings context property returned: ${redundant_context_property}")
    endif()
endforeach()

set(core_production_cpp_files
    core/SettingsController.cpp
    services/wallpaper/SettingsWallpaperController.cpp
    core/navigation/SettingsNavigationCatalog.cpp
    core/navigation/SettingsNavigationModel.cpp
    platform/linux/AdminGroupDetector.cpp
    platform/linux/AdministrativeGroupPolicy.cpp
    services/assets/SettingsIconResolver.cpp
    services/i18n/SettingsTranslationController.cpp
    services/profile/SettingsUserProfileProvider.cpp
    services/dock/SettingsDockController.cpp
)
foreach(relative_path IN LISTS core_production_cpp_files)
    string(FIND "${settings_tests_cmake}" "${relative_path}" repeated_position)
    if(NOT repeated_position EQUAL -1)
        message(FATAL_ERROR "Test CMake repeats core production source: ${relative_path}")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/Main.qml" main_source)
foreach(forbidden_route IN ITEMS
    "selectedSectionId === \"compositor\""
    "pages/system/Compositor.qml"
)
    string(FIND "${main_source}" "${forbidden_route}" route_position)
    if(NOT route_position EQUAL -1)
        message(FATAL_ERROR "Main.qml hardcodes a Compositor route: ${forbidden_route}")
    endif()
endforeach()

foreach(window_invariant IN ITEMS
    "height: Math.min(760, Screen.desktopAvailableHeight - 32)"
    "minimumHeight: 650"
    "maximumHeight: Screen.desktopAvailableHeight - 16"
)
    string(FIND "${main_source}" "${window_invariant}" window_invariant_position)
    if(window_invariant_position EQUAL -1)
        message(FATAL_ERROR "Main.qml is missing window sizing invariant: ${window_invariant}")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/components/form/SettingRow.qml" setting_row_source)
foreach(setting_row_invariant IN ITEMS
    "spacing: Components.Theme.spacingMicro"
    "implicitHeight: Math.max(sr.sublabel !== \"\" ? 64 : 52, rowLayout.implicitHeight + Components.Theme.spacingMedium * 2)"
)
    string(FIND "${setting_row_source}" "${setting_row_invariant}" setting_row_invariant_position)
    if(setting_row_invariant_position EQUAL -1)
        message(FATAL_ERROR "SettingRow.qml is missing shared geometry invariant: ${setting_row_invariant}")
    endif()
endforeach()

set(production_source_files
    core/SettingsController.cpp
    core/SettingsController.hpp
    services/wallpaper/SettingsWallpaperController.hpp
    core/navigation/SettingsNavigationCatalog.cpp
    core/navigation/SettingsNavigationCatalog.hpp
    core/navigation/SettingsNavigationEntry.hpp
    core/navigation/SettingsNavigationModel.cpp
    core/navigation/SettingsNavigationModel.hpp
    platform/linux/AdminGroupDetector.cpp
    platform/linux/AdminGroupDetector.hpp
    platform/linux/AdministrativeGroupPolicy.cpp
    platform/linux/AdministrativeGroupPolicy.hpp
    services/assets/SettingsIconResolver.cpp
    services/assets/SettingsIconResolver.hpp
    services/i18n/SettingsTranslationController.cpp
    services/i18n/SettingsTranslationController.hpp
    services/profile/SettingsUserProfile.hpp
    services/profile/SettingsUserProfileProvider.cpp
    services/profile/SettingsUserProfileProvider.hpp
    services/dock/SettingsDockController.cpp
    services/dock/SettingsDockController.hpp
    app/main.cpp
    app/SettingsApplication.cpp
    app/SettingsApplication.hpp
    qml/Main.qml
    qml/components/AppIcon.qml
    qml/components/navigation/NavItem.qml
    qml/components/navigation/Sidebar.qml
    qml/pages/appearance/Wallpaper.qml
    qml/pages/system/Compositor.qml
)
list(APPEND production_source_files qml/pages/appearance/Dock.qml)

set(required_dock_production_sources
    services/dock/SettingsDockController.cpp
    services/dock/SettingsDockController.hpp
    qml/pages/appearance/Dock.qml
)
foreach(relative_path IN LISTS required_dock_production_sources)
    list(FIND production_source_files "${relative_path}" required_source_index)
    if(required_source_index EQUAL -1)
        message(FATAL_ERROR "Dock production source is missing from the architecture guard: ${relative_path}")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/components/navigation/Sidebar.qml" sidebar_source)
foreach(sidebar_required_token IN ITEMS
    "itemKind === \"section\""
    "itemKind === \"child\""
    "root.model.toggleSection(model.entryId)"
    "leftInset:  navDelegate.itemKind === \"child\" ? 24 : 0"
    "compact:    navDelegate.itemKind === \"child\""
)
    string(FIND "${sidebar_source}" "${sidebar_required_token}" sidebar_required_position)
    if(sidebar_required_position EQUAL -1)
        message(FATAL_ERROR "Sidebar is missing canonical navigation contract '${sidebar_required_token}'")
    endif()
endforeach()
foreach(sidebar_forbidden_token IN ITEMS
    "openUserProfile"
    "setProperty(index"
    "pageIndex"
)
    string(FIND "${sidebar_source}" "${sidebar_forbidden_token}" sidebar_forbidden_position)
    if(NOT sidebar_forbidden_position EQUAL -1)
        message(FATAL_ERROR "Forbidden Sidebar migration behavior returned: ${sidebar_forbidden_token}")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/components/AppIcon.qml" app_icon_source)
string(FIND "${app_icon_source}" "image://astrea-icon/" app_icon_provider_position)
if(app_icon_provider_position EQUAL -1)
    message(FATAL_ERROR "Settings AppIcon must use the registered astrea-icon provider")
endif()
string(FIND "${app_icon_source}" "image://icon/" wrong_app_icon_provider_position)
if(NOT wrong_app_icon_provider_position EQUAL -1)
    message(FATAL_ERROR "Settings AppIcon uses the unregistered icon provider")
endif()

file(READ "${SETTINGS_SOURCE_DIR}/qml/pages/appearance/Wallpaper.qml" wallpaper_controls_source)
foreach(deferred_wallpaper_control IN ITEMS
    "objectName: \"allWorkspacesToggle\""
    "objectName: \"blurredWallpaperToggle\""
    "objectName: \"transitionSelector\""
    "enabled: false"
)
    string(FIND "${wallpaper_controls_source}" "${deferred_wallpaper_control}" deferred_control_position)
    if(deferred_control_position EQUAL -1)
        message(FATAL_ERROR "Wallpaper deferred-control contract is missing '${deferred_wallpaper_control}'")
    endif()
endforeach()
string(FIND "${wallpaper_controls_source}" "selectedTransition" selected_transition_position)
if(NOT selected_transition_position EQUAL -1)
    message(FATAL_ERROR "Wallpaper retains unsupported mutable transition preview state")
endif()

set(forbidden_production_tokens
    "import Quickshell"
    "Quickshell.Io"
    "LayerShellQt"
    "LayerShellHelper"
    "hyprctl"
    "QProcess"
    "Process {"
    "system("
    "popen("
    "pageIndex"
    "Typhon"
)
foreach(relative_path IN LISTS production_source_files)
    file(READ "${SETTINGS_SOURCE_DIR}/${relative_path}" source_text)
    foreach(token IN LISTS forbidden_production_tokens)
        string(FIND "${source_text}" "${token}" token_position)
        if(NOT token_position EQUAL -1)
            message(FATAL_ERROR "Forbidden token '${token}' found in production source ${relative_path}")
        endif()
    endforeach()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/pages/appearance/Dock.qml" dock_qml_source)
foreach(dock_qml_forbidden_token IN ITEMS
    "import Quickshell"
    "Quickshell.Io"
    "Process {"
    "QProcess"
    "File"
    "FileView"
    "FileDialog"
    "FolderListModel"
    "Qt.labs.folderlistmodel"
    "Qt.labs.settings"
    "Settings {"
    "import QtDBus"
    "QtDBus"
    "DBus"
    "QDBus"
    "Socket"
    "LocalSocket"
    "Datagram"
    "IPC"
    "DockIpc"
    "ShellIpc"
    "LayerShell"
    "LayerShellQt"
    "LayerShellHelper"
    "Hyprland"
    "hyprctl"
    "Typhon"
)
    string(FIND "${dock_qml_source}" "${dock_qml_forbidden_token}" dock_qml_token_position)
    if(NOT dock_qml_token_position EQUAL -1)
        message(FATAL_ERROR "Forbidden token '${dock_qml_forbidden_token}' found in Dock.qml")
    endif()
endforeach()

foreach(feedback_file IN ITEMS DnsPresetChip DnsStatusCard ProgressCard SpeedCard StatusDot)
    file(READ "${SETTINGS_SOURCE_DIR}/qml/components/feedback/${feedback_file}.qml" feedback_source)
    string(FIND "${feedback_source}" "import \"./feedback\"" recursive_import_position)
    if(NOT recursive_import_position EQUAL -1)
        message(FATAL_ERROR "Feedback component recursively imports its own directory: ${feedback_file}")
    endif()
endforeach()

message(STATUS "Settings structure invariants passed")
