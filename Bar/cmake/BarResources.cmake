set(ASTREA_BAR_QML_FILES
    "${CMAKE_CURRENT_LIST_DIR}/../qml/ReserveSurface.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/LauncherSurface.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/StatusSurface.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/Tray.qml"
    "${CMAKE_CURRENT_LIST_DIR}/../qml/TrayContextMenu.qml"
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
)
set(ASTREA_BAR_RESOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../assets/astrea.png"
)

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
    "${CMAKE_CURRENT_LIST_DIR}/../assets/astrea.png"
    PROPERTIES QT_RESOURCE_ALIAS "Bar/assets/astrea.png")

function(astrea_add_bar_qml_resources target)
    qt_add_resources(${target} astrea_bar_resources
        PREFIX "/qt/qml/Astrea/Shell"
        FILES ${ASTREA_BAR_QML_FILES} ${ASTREA_BAR_RESOURCES}
    )
endfunction()
