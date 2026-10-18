# The registered sources are recomputed from the authoritative list whenever
# CMake is rerun (including renames in an existing build tree).
unset(ASTREA_SETTINGS_QML_FILES CACHE)
set(ASTREA_SETTINGS_QML_RELATIVE_FILES
    Main.qml
    components/AppIcon.qml
    components/AvatarImage.qml
    components/Theme.qml
    components/audio/DeviceRow.qml
    components/controls/Button.qml
    components/controls/ButtonCapsule.qml
    components/controls/DualButton.qml
    components/controls/FloatingButton.qml
    components/controls/ToggleSwitch.qml
    components/controls/SelectButton.qml
    components/controls/SearchField.qml
    components/controls/Slider.qml
    components/feedback/DnsPresetChip.qml
    components/feedback/DnsStatusCard.qml
    components/feedback/ProgressCard.qml
    components/feedback/SpeedCard.qml
    components/feedback/StatusDot.qml
    components/form/FormCard.qml
    components/form/IconListRow.qml
    components/form/ScrollPage.qml
    components/form/SectionHeader.qml
    components/form/SettingRow.qml
    components/menu/ContextMenu.qml
    components/menu/ContextMenuAction.qml
    components/menu/ContextMenuDivider.qml
    components/navigation/NavItem.qml
    components/navigation/HubNavigationRow.qml
    components/navigation/Sidebar.qml
    components/navigation/SidebarFrame.qml
    components/typography/DisplayLabel.qml
    components/typography/Divider.qml
    components/typography/TextLabel.qml
    pages/navigation/Hub.qml
    pages/system/Compositor.qml
    pages/system/Bluetooth.qml
    pages/system/Audio.qml
    pages/system/Display.qml
    pages/system/Internet.qml
    pages/appearance/Appearance.qml
    pages/appearance/VisualEffects.qml
    pages/appearance/MaterialProgramParameterEditor.qml
    pages/appearance/Icons.qml
    pages/appearance/MaterialPreview.qml
    pages/appearance/MaterialShowcase.qml
    pages/appearance/Dock.qml
    pages/appearance/Animations.qml
    pages/appearance/Screenshots.qml
    pages/appearance/Wallpaper.qml
    theme/Apps.qml
    theme/Shell.qml
    theme/State.qml
    theme/Tokens.qml
)

set(ASTREA_SETTINGS_ASSET_RELATIVE_FILES
    i18n/en_US.json
    fonts/tabler-icons-300.ttf
    icons/navigation/lucide/system.svg
    icons/navigation/lucide/software-update.svg
    icons/navigation/lucide/internet.svg
    icons/navigation/lucide/bluetooth.svg
    icons/navigation/lucide/audio.svg
    icons/navigation/lucide/display.svg
    icons/navigation/lucide/components.svg
    icons/navigation/lucide/services.svg
    icons/navigation/lucide/compositor.svg
    icons/navigation/lucide/performance.svg
    icons/navigation/lucide/customization.svg
    icons/navigation/lucide/appearance.svg
    icons/navigation/lucide/visual-effects.svg
    icons/navigation/lucide/icons.svg
    icons/navigation/lucide/wallpaper.svg
    icons/navigation/lucide/dock.svg
    icons/navigation/lucide/animations.svg
    icons/navigation/lucide/screenshots.svg
    icons/navigation/lucide/more-settings.svg
    icons/navigation/lucide/microphone.svg
    icons/navigation/lucide/headphones.svg
    icons/navigation/lucide/virtual-device.svg
    icons/settings/audio.svg
    icons/settings/apps.svg
    icons/settings/bluetooth.svg
    icons/settings/display.svg
    icons/settings/network.svg
    icons/settings/performance.svg
    icons/settings/software-center.svg
    icons/settings/sound.svg
    icons/settings/theme.svg
    icons/settings/wallpaper.svg
    icons/settings/animations.svg
    icons/settings/themes/bluetooth.svg
    icons/settings/themes/dark/audio.svg
    icons/settings/themes/dark/apps.svg
    icons/settings/themes/dark/bluetooth.svg
    icons/settings/themes/dark/display.svg
    icons/settings/themes/dark/network.svg
    icons/settings/themes/dark/performance.svg
    icons/settings/themes/dark/software-center.svg
    icons/settings/themes/dark/theme.svg
    icons/settings/themes/dark/wallpaper.svg
    previews/screenshot-preview.svg
)

# Generated bundled Lucide SVG weight resources.
set(ASTREA_LUCIDE_WEIGHT_NAMES ultralight thin light regular medium semibold bold heavy black)
set(ASTREA_LUCIDE_NAV_ICON_KEYS system software-update internet bluetooth audio display components services compositor performance customization appearance visual-effects icons wallpaper dock animations screenshots more-settings microphone headphones virtual-device)
foreach(weight IN LISTS ASTREA_LUCIDE_WEIGHT_NAMES)
    foreach(icon IN LISTS ASTREA_LUCIDE_NAV_ICON_KEYS)
        list(APPEND ASTREA_SETTINGS_ASSET_RELATIVE_FILES "icons/navigation/lucide/weights/${weight}/${icon}.svg")
    endforeach()
endforeach()

set(ASTREA_SETTINGS_QML_FILES)
foreach(qml_file IN LISTS ASTREA_SETTINGS_QML_RELATIVE_FILES)
    get_filename_component(qml_source
        "${CMAKE_CURRENT_SOURCE_DIR}/../qml/${qml_file}" ABSOLUTE)
    set_source_files_properties(${qml_source} PROPERTIES QT_RESOURCE_ALIAS "qml/${qml_file}")
    list(APPEND ASTREA_SETTINGS_QML_FILES "${qml_source}")
endforeach()

get_filename_component(ASTREA_SETTINGS_SCREENSHOT_VISUAL_QML
    "${CMAKE_CURRENT_SOURCE_DIR}/../../shared/qml/ScreenshotVisual.qml" ABSOLUTE)
set_source_files_properties(${ASTREA_SETTINGS_SCREENSHOT_VISUAL_QML}
    PROPERTIES QT_RESOURCE_ALIAS "qml/components/ScreenshotVisual.qml")
list(APPEND ASTREA_SETTINGS_QML_FILES ${ASTREA_SETTINGS_SCREENSHOT_VISUAL_QML})

unset(ASTREA_SETTINGS_ASSET_FILES CACHE)
set(ASTREA_SETTINGS_ASSET_FILES "")
foreach(asset_file IN LISTS ASTREA_SETTINGS_ASSET_RELATIVE_FILES)
    get_filename_component(asset_source
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets/${asset_file}" ABSOLUTE)
    set_source_files_properties(${asset_source} PROPERTIES QT_RESOURCE_ALIAS "assets/${asset_file}")
    list(APPEND ASTREA_SETTINGS_ASSET_FILES "${asset_source}")
endforeach()

set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/../qml/theme/Tokens.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../qml/theme/Apps.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../qml/theme/Shell.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../qml/theme/State.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/../qml/components/Theme.qml"
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

add_library(astrea-settings-ui STATIC)
target_include_directories(astrea-settings-ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(astrea-settings-ui PUBLIC
    astrea-shared-system-qt
    Qt6::Core
    Qt6::Core5Compat
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

qt_add_qml_module(astrea-settings-ui
    URI Astrea.Settings
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Astrea/Settings"
    QML_FILES ${ASTREA_SETTINGS_QML_FILES}
)
qt_add_resources(astrea-settings-ui settings-assets
    PREFIX "/Astrea/Settings"
    FILES ${ASTREA_SETTINGS_ASSET_FILES}
)

# Keep the authoritative lists available to structural tests without repeating them.
set(ASTREA_SETTINGS_QML_FILES "${ASTREA_SETTINGS_QML_FILES}" CACHE INTERNAL "Registered Settings QML files" FORCE)
set(ASTREA_SETTINGS_QML_RELATIVE_FILES "${ASTREA_SETTINGS_QML_RELATIVE_FILES}" CACHE INTERNAL "Registered Settings QML paths" FORCE)
set(ASTREA_SETTINGS_ASSET_FILES "${ASTREA_SETTINGS_ASSET_FILES}" CACHE INTERNAL "Registered Settings assets" FORCE)

settings_enable_warnings(astrea-settings-ui)
settings_enable_sanitizers(astrea-settings-ui)
