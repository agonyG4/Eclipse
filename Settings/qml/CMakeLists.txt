set(ASTREA_SETTINGS_QML_RELATIVE_FILES
    Main.qml
    components/AppIcon.qml
    components/AvatarImage.qml
    components/Theme.qml
    components/controls/Button.qml
    components/controls/ButtonCapsule.qml
    components/controls/DualButton.qml
    components/controls/FloatingButton.qml
    components/feedback/DnsPresetChip.qml
    components/feedback/DnsStatusCard.qml
    components/feedback/ProgressCard.qml
    components/feedback/SpeedCard.qml
    components/feedback/StatusDot.qml
    components/form/FormCard.qml
    components/form/IconListRow.qml
    components/form/ScrollPage.qml
    components/form/SearchField.qml
    components/form/SectionHeader.qml
    components/form/SelectButton.qml
    components/form/SettingRow.qml
    components/form/ToggleSwitch.qml
    components/menu/ContextMenu.qml
    components/menu/ContextMenuAction.qml
    components/menu/ContextMenuDivider.qml
    components/navigation/NavItem.qml
    components/navigation/Sidebar.qml
    components/navigation/SidebarFrame.qml
    components/typography/DisplayLabel.qml
    components/typography/Divider.qml
    components/typography/TextLabel.qml
    pages/system/Compositor.qml
    pages/appearance/Dock.qml
    pages/appearance/Wallpaper.qml
    theme/Apps.qml
    theme/Shell.qml
    theme/State.qml
    theme/Tokens.qml
)

set(ASTREA_SETTINGS_ASSET_RELATIVE_FILES
    i18n/en_US.json
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
)

set(ASTREA_SETTINGS_QML_FILES)
foreach(qml_file IN LISTS ASTREA_SETTINGS_QML_RELATIVE_FILES)
    set(qml_source "${CMAKE_CURRENT_SOURCE_DIR}/../qml/${qml_file}")
    set_source_files_properties(${qml_source} PROPERTIES QT_RESOURCE_ALIAS "qml/${qml_file}")
    list(APPEND ASTREA_SETTINGS_QML_FILES "${qml_source}")
endforeach()

set(ASTREA_SETTINGS_ASSET_FILES)
foreach(asset_file IN LISTS ASTREA_SETTINGS_ASSET_RELATIVE_FILES)
    set(asset_source "${CMAKE_CURRENT_SOURCE_DIR}/../assets/${asset_file}")
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
set(ASTREA_SETTINGS_QML_FILES "${ASTREA_SETTINGS_QML_FILES}" CACHE INTERNAL "Registered Settings QML files")
set(ASTREA_SETTINGS_QML_RELATIVE_FILES "${ASTREA_SETTINGS_QML_RELATIVE_FILES}" CACHE INTERNAL "Registered Settings QML paths")
set(ASTREA_SETTINGS_ASSET_FILES "${ASTREA_SETTINGS_ASSET_FILES}" CACHE INTERNAL "Registered Settings assets")

settings_enable_warnings(astrea-settings-ui)
settings_enable_sanitizers(astrea-settings-ui)
