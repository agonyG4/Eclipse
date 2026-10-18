set(ASTREA_BAR_QML_FILES
    "${CMAKE_CURRENT_LIST_DIR}/../qml/ReserveSurface.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/LauncherSurface.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/StatusSurface.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/Tray.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/TrayContextMenu.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/TrayMenuCard.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/TrayTooltipSurface.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/PopupOverlaySurface.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/AstreaMenu.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/NetworkPopup.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/BluetoothPopup.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/VolumePopup.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/ShellBarTheme.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/BarSegment.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/IndicatorButton.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/TopbarIndicator.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/WorkspaceStrip.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/Clock.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/MenuAction.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/MenuItem.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/MenuSeparator.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/PopupHeader.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/PopupCard.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/NetworkIndicator.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/BluetoothIndicator.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/VolumeIndicator.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/VolumeStateIcon.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/AppleIcon.qml"
)
set(ASTREA_BAR_RESOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/wifi.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/wifi-off.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/wired.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/bluetooth.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/bluetooth-off.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/volume-high.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/volume-low.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/volume-muted.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/search.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/about.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/settings.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/force-quit.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/lock.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/power.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/download.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/upload.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/refresh.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/chevron.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/check.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/radio.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/headphones.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/phone.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/monitor.svg"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/astrea.png"
)

# Generated Lucide SVG weight resources.
set(ASTREA_BAR_LUCIDE_WEIGHT_NAMES ultralight thin light regular medium semibold bold heavy black)
set(ASTREA_BAR_LUCIDE_ICON_KEYS wifi wifi-off wired bluetooth bluetooth-off volume-high volume-low volume-muted search about settings force-quit lock power download upload refresh chevron check radio headphones phone monitor)
foreach(weight IN LISTS ASTREA_BAR_LUCIDE_WEIGHT_NAMES)
    foreach(icon IN LISTS ASTREA_BAR_LUCIDE_ICON_KEYS)
        set(icon_file "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/weights/${weight}/${icon}.svg")
        set_source_files_properties("${icon_file}"
            PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/weights/${weight}/${icon}.svg")
        list(APPEND ASTREA_BAR_RESOURCES "${icon_file}")
    endforeach()
endforeach()

set_source_files_properties(${ASTREA_BAR_QML_FILES} ${ASTREA_BAR_RESOURCES}
    PROPERTIES QT_RESOURCE_PREFIX "/qt/qml/Astrea/Shell")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/ReserveSurface.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/ReserveSurface.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/LauncherSurface.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/LauncherSurface.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/StatusSurface.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/StatusSurface.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/Tray.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/Tray.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/TrayContextMenu.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/TrayContextMenu.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/TrayMenuCard.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/TrayMenuCard.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/TrayTooltipSurface.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/TrayTooltipSurface.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/PopupOverlaySurface.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/PopupOverlaySurface.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/AstreaMenu.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/AstreaMenu.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/NetworkPopup.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/NetworkPopup.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/BluetoothPopup.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/BluetoothPopup.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/VolumePopup.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/VolumePopup.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/ShellBarTheme.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/ShellBarTheme.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/VolumeStateIcon.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/VolumeStateIcon.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/AppleIcon.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/AppleIcon.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/BarSegment.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/BarSegment.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/IndicatorButton.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/IndicatorButton.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/TopbarIndicator.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/TopbarIndicator.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/WorkspaceStrip.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/WorkspaceStrip.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/Clock.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/Clock.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/MenuAction.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/MenuAction.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/MenuItem.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/MenuItem.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/MenuSeparator.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/MenuSeparator.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/PopupHeader.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/PopupHeader.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/PopupCard.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/PopupCard.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/NetworkIndicator.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/NetworkIndicator.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/BluetoothIndicator.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/BluetoothIndicator.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../qml/components/VolumeIndicator.qml"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/qml/components/VolumeIndicator.qml")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/wifi.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/wifi.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/wifi-off.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/wifi-off.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/wired.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/wired.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/bluetooth.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/bluetooth.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/bluetooth-off.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/bluetooth-off.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/volume-high.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/volume-high.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/volume-low.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/volume-low.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/volume-muted.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/volume-muted.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/search.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/search.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/about.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/about.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/settings.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/settings.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/force-quit.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/force-quit.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/lock.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/lock.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/power.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/power.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/download.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/download.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/upload.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/upload.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/refresh.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/refresh.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/chevron.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/chevron.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/check.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/check.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/radio.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/radio.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/headphones.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/headphones.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/phone.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/phone.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/icons/lucide/monitor.svg"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/icons/lucide/monitor.svg")
set_source_files_properties(
    "${CMAKE_CURRENT_LIST_DIR}/../assets/astrea.png"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/assets/astrea.png")

function(astrea_add_bar_qml_resources target)
    qt_add_resources(${target} astrea_bar_resources
        PREFIX "/qt/qml/Astrea/Shell"
        FILES ${ASTREA_BAR_QML_FILES} ${ASTREA_BAR_RESOURCES}
    )
endfunction()
