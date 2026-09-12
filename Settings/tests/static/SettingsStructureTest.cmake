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

file(READ "${SETTINGS_SOURCE_DIR}/core/CMakeLists.txt" settings_core_cmake)
file(READ "${SETTINGS_SOURCE_DIR}/tests/CMakeLists.txt" settings_tests_cmake)
file(READ "${SETTINGS_SOURCE_DIR}/app/SettingsApplication.cpp" settings_application_source)
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
foreach(main_forbidden_token IN ITEMS
    "selectedSection"
    "selectSection"
    "pages/system/Compositor.qml"
    "pages/appearance/Wallpaper.qml"
    "pages/appearance/Dock.qml"
)
    string(FIND "${main_source}" "${main_forbidden_token}" main_token_position)
    if(NOT main_token_position EQUAL -1)
        message(FATAL_ERROR "Main.qml contains forbidden navigation detail '${main_forbidden_token}'")
    endif()
endforeach()
foreach(main_required_token IN ITEMS
    "SettingsController.selectedSidebarId"
    "SettingsController.navigateTo"
    "SettingsController.canGoBack"
    "SettingsController.canGoForward"
    "SettingsController.goBack"
    "SettingsController.goForward"
    "settingsNavigationToolbar"
    "settingsBackForwardControl"
)
    string(FIND "${main_source}" "${main_required_token}" main_required_position)
    if(main_required_position EQUAL -1)
        message(FATAL_ERROR "Main.qml is missing native navigation contract '${main_required_token}'")
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
    qml/components/navigation/HubNavigationRow.qml
    qml/components/navigation/Sidebar.qml
    qml/pages/navigation/Hub.qml
    qml/pages/appearance/Appearance.qml
    qml/pages/appearance/MaterialPreview.qml
    qml/pages/appearance/MaterialShowcase.qml
    qml/pages/appearance/Wallpaper.qml
    qml/pages/system/Compositor.qml
    qml/pages/appearance/Dock.qml
)

foreach(relative_path IN LISTS production_source_files)
    if(NOT EXISTS "${SETTINGS_SOURCE_DIR}/${relative_path}")
        message(FATAL_ERROR "Settings production source is missing: ${relative_path}")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/core/navigation/SettingsNavigationEntry.hpp" navigation_entry_source)
file(READ "${SETTINGS_SOURCE_DIR}/core/navigation/SettingsNavigationCatalog.cpp" navigation_catalog_source)
file(READ "${SETTINGS_SOURCE_DIR}/core/navigation/SettingsNavigationModel.hpp" navigation_model_header)
file(READ "${SETTINGS_SOURCE_DIR}/core/navigation/SettingsNavigationModel.cpp" navigation_model_source)
file(READ "${SETTINGS_SOURCE_DIR}/core/SettingsController.hpp" controller_header)
file(READ "${SETTINGS_SOURCE_DIR}/core/SettingsController.cpp" controller_source)
foreach(navigation_forbidden_token IN ITEMS
    "Kind::Section"
    "Kind::Child"
    "toggleSection"
    "sectionKey"
    "parentSection"
    "expanded"
    "SelectedRole"
    "selectedSection"
    "selectSection"
)
    foreach(navigation_source IN ITEMS
        navigation_entry_source navigation_catalog_source navigation_model_header
        navigation_model_source controller_header controller_source
    )
        string(FIND "${${navigation_source}}" "${navigation_forbidden_token}" navigation_token_position)
        if(NOT navigation_token_position EQUAL -1)
            message(FATAL_ERROR "Obsolete navigation token '${navigation_forbidden_token}' found in ${navigation_source}")
        endif()
    endforeach()
endforeach()
foreach(navigation_required_token IN ITEMS
    "Kind::Hub"
    "sidebarVisible"
    "parentId"
    "appearance"
    "Appearance.qml"
    "settings.nav.appearance.subtitle"
    "childrenForId"
    "firstNavigableSidebarDestination"
    "sidebarAncestorForId"
)
    string(FIND "${navigation_entry_source}${navigation_catalog_source}${navigation_model_header}${navigation_model_source}"
        "${navigation_required_token}" navigation_required_position)
    if(navigation_required_position EQUAL -1)
        message(FATAL_ERROR "Native destination graph is missing '${navigation_required_token}'")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/pages/appearance/Appearance.qml" appearance_source)
foreach(appearance_required_token IN ITEMS
    "objectName: \"appearancePage\""
    "objectName: \"appearanceScrollPage\""
    "objectName: \"appearanceOption-auto\""
    "objectName: \"appearanceOption-light\""
    "objectName: \"appearanceOption-dark\""
    "objectName: \"interfaceStyleOption-default\""
    "objectName: \"interfaceStyleOption-transparent\""
    "objectName: \"interfaceStyleOption-frosted\""
    "objectName: \"accentOption-blue\""
    "objectName: \"accentOption-purple\""
    "objectName: \"accentOption-red\""
    "objectName: \"accentOption-orange\""
    "objectName: \"accentOption-yellow\""
    "objectName: \"accentOption-green\""
    "objectName: \"accentOption-teal\""
    "objectName: \"iconAppearance-default\""
    "objectName: \"iconAppearance-monochrome\""
    "SettingsController.wallpaper"
    "wallpaperController.effectivePreviewUrl"
    "wallpaperController.effectiveFit"
    "wallpaperController.refresh()"
    "objectName: \"iconAppearance-tinted\""
    "apps.settings.pages.appearance.text.appearance"
    "apps.settings.pages.appearance.text.interface_style"
    "apps.settings.pages.appearance.text.accent_color"
    "apps.settings.pages.appearance.text.app_icons"
    "apps.settings.pages.appearance.option.monochrome"
    "apps.settings.pages.appearance.option.tinted"
)
    string(FIND "${appearance_source}" "${appearance_required_token}" appearance_required_position)
    if(appearance_required_position EQUAL -1)
        message(FATAL_ERROR "Appearance page is missing '${appearance_required_token}'")
    endif()
endforeach()

foreach(appearance_forbidden_token IN ITEMS
    "interfaceBackdropOpacity"
    "previewWindow"
    "previewLightBackground"
    "previewDarkBackground"
    "#4d75a8"
    "#9a62ba"
    "#2ea4a4"
)
    string(FIND "${appearance_source}" "${appearance_forbidden_token}" appearance_forbidden_position)
    if(NOT appearance_forbidden_position EQUAL -1)
        message(FATAL_ERROR "Appearance.qml still owns preview rendering token '${appearance_forbidden_token}'")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/components/navigation/Sidebar.qml" sidebar_source)
foreach(sidebar_forbidden_token IN ITEMS
    "section"
    "child"
    "toggleSection"
    "leftInset"
    "compact"
    "sectionMouse"
    "expanded"
)
    string(FIND "${sidebar_source}" "${sidebar_forbidden_token}" sidebar_token_position)
    if(NOT sidebar_token_position EQUAL -1)
        message(FATAL_ERROR "Flat Sidebar contains obsolete expansion behavior '${sidebar_forbidden_token}'")
    endif()
endforeach()
foreach(sidebar_required_token IN ITEMS
    "root.model"
    "root.selectedId"
    "root.selectId(model.entryId)"
    "settingsSidebar"
)
    string(FIND "${sidebar_source}" "${sidebar_required_token}" sidebar_required_position)
    if(sidebar_required_position EQUAL -1)
        message(FATAL_ERROR "Flat Sidebar is missing '${sidebar_required_token}'")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/pages/navigation/Hub.qml" hub_source)
foreach(hub_required_token IN ITEMS
    "settingsHubPage"
    "SettingsController.currentDestination"
    "SettingsController.currentDestinationChildren"
    "SettingsController.navigateTo(descriptor.entryId)"
    "Layout.bottomMargin: Components.Theme.spacingLarge"
    "function iconSourceFor"
    "SettingsController.iconUrl(iconKey, Theme.iconTheme)"
    "iconSource"
)
    string(FIND "${hub_source}" "${hub_required_token}" hub_required_position)
    if(hub_required_position EQUAL -1)
        message(FATAL_ERROR "Generic Hub is missing '${hub_required_token}'")
    endif()
endforeach()
foreach(hub_forbidden_token IN ITEMS
    "Wallpaper.qml"
    "Dock.qml"
    "currentDestinationId ==="
    "if (currentDestination"
)
    string(FIND "${hub_source}" "${hub_forbidden_token}" hub_forbidden_position)
    if(NOT hub_forbidden_position EQUAL -1)
        message(FATAL_ERROR "Generic Hub contains hardcoded destination behavior '${hub_forbidden_token}'")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/components/navigation/HubNavigationRow.qml" hub_row_source)
foreach(hub_row_required_token IN ITEMS "destinationId" "signal clicked" "chevron")
    string(FIND "${hub_row_source}" "${hub_row_required_token}" hub_row_required_position)
    if(hub_row_required_position EQUAL -1)
        message(FATAL_ERROR "Hub navigation row is missing '${hub_row_required_token}'")
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

file(READ "${SETTINGS_SOURCE_DIR}/qml/components/AppIcon.qml" app_icon_source)
string(FIND "${app_icon_source}" "image://astrea-icon/" app_icon_provider_position)
if(app_icon_provider_position EQUAL -1)
    message(FATAL_ERROR "Settings AppIcon must use the registered astrea-icon provider")
endif()
string(FIND "${app_icon_source}" "image://icon/" wrong_app_icon_provider_position)
if(NOT wrong_app_icon_provider_position EQUAL -1)
    message(FATAL_ERROR "Settings AppIcon uses the unregistered icon provider")
endif()

file(READ "${SETTINGS_SOURCE_DIR}/qml/pages/appearance/MaterialPreview.qml" material_preview_source)
foreach(material_preview_required_token IN ITEMS
    "objectName: \"materialPreview\""
    "objectName: \"materialPreviewWallpaper\""
    "objectName: \"materialPreviewFallback\""
    "property url wallpaperSource"
    "property string wallpaperFit"
    "import QtQuick.Window"
    "rendererPreviewReady"
    "rendererPreviewRequested"
    "usingRendererPreview"
    "rendererFrame.status === Image.Ready"
    "rendererPreviewFailed"
    "Image.PreserveAspectCrop"
    "Image.PreserveAspectFit"
    "Image.Stretch"
    "Image.Pad"
    "Image.Tile"
    "import Astrea.Effects"
    "anchors.centerIn: parent"
    "width: parent.width * 0.70"
    "height: parent.height * 0.64"
    "liveFrosted.effectActive"
)
    string(FIND "${material_preview_source}" "${material_preview_required_token}" material_preview_token_position)
    if(material_preview_token_position EQUAL -1)
        message(FATAL_ERROR "MaterialPreview.qml is missing '${material_preview_required_token}'")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/pages/appearance/MaterialShowcase.qml" material_showcase_source)
foreach(material_showcase_required_token IN ITEMS
    "objectName: canonicalIdentity ? \"materialPreviewShowcase\" : \"materialPreviewShowcaseFallback\""
    "property bool canonicalIdentity"
    "materialPreviewShowcaseFallback"
    "property string themeVariant"
    "property string materialId"
    "Rectangle {"
)
    string(FIND "${material_showcase_source}" "${material_showcase_required_token}" material_showcase_token_position)
    if(material_showcase_token_position EQUAL -1)
        message(FATAL_ERROR "MaterialShowcase.qml is missing '${material_showcase_required_token}'")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/../shared/platform/wayland/effects/AstreaWaylandEffects.cpp"
    wayland_effects_source)
string(REGEX MATCHALL "return m_available;" wayland_effects_returns
    "${wayland_effects_source}")
list(LENGTH wayland_effects_returns wayland_effects_return_count)
if(wayland_effects_return_count LESS 2)
    message(FATAL_ERROR "Wayland effects initialization must return actual availability")
endif()
string(FIND "${wayland_effects_source}" "wl_surface_commit(" raw_wayland_commit_position)
if(NOT raw_wayland_commit_position EQUAL -1)
    message(FATAL_ERROR "Public Wayland effects must not commit Qt-owned wl_surface objects")
endif()
string(FIND "${wayland_effects_source}" "wl_display_connect(" raw_wayland_display_connect_position)
if(NOT raw_wayland_display_connect_position EQUAL -1)
    message(FATAL_ERROR "Public Wayland effects must reuse Qt's existing display connection")
endif()
string(FIND "${wayland_effects_source}" "m_surfaces" global_surface_map_position)
if(NOT global_surface_map_position EQUAL -1)
    message(FATAL_ERROR "Per-surface effect proxies must not be owned by the application manager")
endif()
foreach(wayland_effects_required_token IN ITEMS
    "wl_display_roundtrip(m_display)"
    "updateCapabilities(flags)"
    "updateAvailability()"
    "requestQtFrame(window)"
)
    string(FIND "${wayland_effects_source}" "${wayland_effects_required_token}"
        wayland_effects_token_position)
    if(wayland_effects_token_position EQUAL -1)
        message(FATAL_ERROR "Wayland effects manager is missing '${wayland_effects_required_token}'")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/../shared/platform/wayland/effects/AstreaEffectSurfaceController.cpp"
    effect_surface_controller_source)
foreach(effect_surface_controller_required_token IN ITEMS
    "const bool initialized = effects && effects->initialize();"
    "m_available = initialized && effects->available();"
)
    string(FIND "${effect_surface_controller_source}" "${effect_surface_controller_required_token}"
        effect_surface_controller_token_position)
    if(effect_surface_controller_token_position EQUAL -1)
        message(FATAL_ERROR "Effect surface controller is missing '${effect_surface_controller_required_token}'")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/CMakeLists.txt" settings_cmake_source)
string(FIND "${settings_cmake_source}"
    "find_package(Qt6 6.8 REQUIRED COMPONENTS Core Core5Compat Gui Network Qml Quick QuickControls2)"
    settings_qt_floor_position)
if(settings_qt_floor_position EQUAL -1)
    message(FATAL_ERROR "Standalone Settings must require Qt 6.8 or newer")
endif()

file(READ "${SETTINGS_SOURCE_DIR}/../shared/CMakeLists.txt" shared_cmake_source)
string(FIND "${shared_cmake_source}"
    "find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick DBus ShaderTools)"
    shared_qt_floor_position)
if(shared_qt_floor_position EQUAL -1)
    message(FATAL_ERROR "Shared Wayland effects must require Qt 6.8 or newer for WindowContainer")
endif()

file(READ "${SETTINGS_SOURCE_DIR}/../shared/platform/wayland/effects/AstreaEffectChildWindow.cpp"
    effect_child_window_source)
foreach(effect_child_window_required_token IN ITEMS
    "format.setAlphaBufferSize(8)"
    "Qt::WindowTransparentForInput"
    "QPlatformSurfaceEvent::SurfaceCreated"
    "QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed"
)
    string(FIND "${effect_child_window_source}" "${effect_child_window_required_token}"
        effect_child_window_token_position)
    if(effect_child_window_token_position EQUAL -1)
        message(FATAL_ERROR "Effect child window is missing '${effect_child_window_required_token}'")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/pages/appearance/Wallpaper.qml" wallpaper_source)
foreach(wallpaper_required_token IN ITEMS
    "objectName: \"allWorkspacesToggle\""
    "objectName: \"blurredWallpaperToggle\""
    "objectName: \"transitionSelector\""
    "objectName: \"wallpaperRemoveDialog\""
    "objectName: \"wallpaperRemoveConfirmButton\""
    "objectName: \"wallpaperTile-\""
    "objectName: \"wallpaperTileImage-\""
    "import \"../../components/menu\" as WallpaperMenu"
    "WallpaperMenu.ContextMenu"
    "objectName: \"wallpaperContextMenu\""
    "objectName: \"wallpaperContextRemoveAction\""
    "acceptedButtons: Qt.LeftButton | Qt.RightButton"
    "mouse.button === Qt.RightButton"
    "contextWallpaperId"
    "source: root.controller.effectivePreviewUrl"
    "source: modelData.previewUrl"
    "sourceSize.width"
    "sourceSize.height"
    "enabled: false"
)
    string(FIND "${wallpaper_source}" "${wallpaper_required_token}" wallpaper_position)
    if(wallpaper_position EQUAL -1)
        message(FATAL_ERROR "Wallpaper deferred-control contract is missing '${wallpaper_required_token}'")
    endif()
endforeach()

foreach(wallpaper_forbidden_token IN ITEMS
    "wallpaperTileRemoveButton"
    "objectName: \"wallpaperRemoveButton\""
    "source: root.controller.effectiveSource"
    "source: modelData.resolvedSource"
    "source: modelData.source"
)
    string(FIND "${wallpaper_source}" "${wallpaper_forbidden_token}" wallpaper_position)
    if(NOT wallpaper_position EQUAL -1)
        message(FATAL_ERROR "Wallpaper QML owns raw source semantics via '${wallpaper_forbidden_token}'")
    endif()
endforeach()

file(READ "${SETTINGS_SOURCE_DIR}/qml/pages/appearance/Dock.qml" dock_source)
foreach(dock_required_token IN ITEMS
    "import \"../../components/controls\" as Controls"
    "Controls.Slider"
    "Controls.ToggleSwitch"
    "Controls.SelectButton"
)
    string(FIND "${dock_source}" "${dock_required_token}" dock_required_position)
    if(dock_required_position EQUAL -1)
        message(FATAL_ERROR "Dock page is missing reusable control contract '${dock_required_token}'")
    endif()
endforeach()
if(dock_source MATCHES "(^|\\n)[ \\t]*Slider[ \\t]*\\{")
    message(FATAL_ERROR "Dock page contains a direct unqualified Slider instance")
endif()
if(dock_source MATCHES "(^|\\n)[ \\t]*import QtQuick\\.Controls([ \\t]*$|[ \\t]*\\n)")
    message(FATAL_ERROR "Dock page retains the unused unqualified Qt Quick Controls import")
endif()

set(settings_control_paths
    components/controls/Slider.qml
    components/controls/ToggleSwitch.qml
    components/controls/SelectButton.qml
    components/controls/SearchField.qml
)
foreach(relative_path IN LISTS settings_control_paths)
    list(FIND registered_qml_files "${relative_path}" registered_index)
    if(registered_index EQUAL -1)
        message(FATAL_ERROR "Reusable control is not registered: ${relative_path}")
    endif()
    if(NOT EXISTS "${SETTINGS_SOURCE_DIR}/qml/${relative_path}")
        message(FATAL_ERROR "Reusable control source is missing: ${relative_path}")
    endif()
endforeach()

set(legacy_form_control_paths
    components/form/ToggleSwitch.qml
    components/form/SelectButton.qml
    components/form/SearchField.qml
)
foreach(relative_path IN LISTS legacy_form_control_paths)
    list(FIND registered_qml_files "${relative_path}" registered_index)
    if(NOT registered_index EQUAL -1)
        message(FATAL_ERROR "Legacy form control remains registered: ${relative_path}")
    endif()
    if(EXISTS "${SETTINGS_SOURCE_DIR}/qml/${relative_path}")
        message(FATAL_ERROR "Legacy form control source remains present: ${relative_path}")
    endif()
endforeach()

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

list(LENGTH registered_qml_files registered_qml_file_count)
if(NOT registered_qml_file_count EQUAL 44)
    message(FATAL_ERROR "Settings module must register exactly 44 QML files, found ${registered_qml_file_count}")
endif()
message(STATUS "Settings structure invariants passed (${registered_qml_file_count} registered QML files checked)")
