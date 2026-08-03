if(NOT DEFINED COMPOSITOR_QML_SOURCE)
    message(FATAL_ERROR "COMPOSITOR_QML_SOURCE is required")
endif()

if(NOT EXISTS "${COMPOSITOR_QML_SOURCE}")
    message(FATAL_ERROR "Compositor.qml is missing: ${COMPOSITOR_QML_SOURCE}")
endif()

file(READ "${COMPOSITOR_QML_SOURCE}" compositor_source)

set(forbidden_tokens
    "import Quickshell"
    "Quickshell.Io"
    "LayerShellQt"
    "hyprctl"
    "QProcess"
    "Process {"
    "system("
    "popen("
    "QSettings"
    "DBus"
    "D-Bus"
    "socket"
    "IPC"
    "Typhon"
    "ThemeController"
    "SettingsController"
)

foreach(token IN LISTS forbidden_tokens)
    string(FIND "${compositor_source}" "${token}" token_position)
    if(NOT token_position EQUAL -1)
        message(FATAL_ERROR "Forbidden token '${token}' found in Compositor.qml")
    endif()
endforeach()

set(required_tokens
    "property bool animationsEnabled: true"
    "property bool blurEnabled: true"
    "property bool shadowsEnabled: true"
    "property int vrrModeIndex: 0"
    "property int tearingPolicyIndex: 0"
    "property bool directScanoutEnabled: true"
    "property int tripleBufferingModeIndex: 0"
    "property bool xwaylandEnabled: true"
    "property int hardwareCursorModeIndex: 0"
    "onToggled: targetChecked =>"
    "onSelected: index =>"
)

foreach(token IN LISTS required_tokens)
    string(FIND "${compositor_source}" "${token}" token_position)
    if(token_position EQUAL -1)
        message(FATAL_ERROR "Required local-only token '${token}' is missing from Compositor.qml")
    endif()
endforeach()
