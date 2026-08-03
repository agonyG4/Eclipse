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

string(REGEX MATCHALL "Form\\.SectionHeader[ \\t]*\\{" section_headers "${compositor_source}")
list(LENGTH section_headers section_header_count)
if(NOT section_header_count EQUAL 4)
    message(FATAL_ERROR "Compositor.qml must keep four section headers")
endif()

string(REGEX MATCHALL "Layout\\.bottomMargin: 12" section_header_margins "${compositor_source}")
list(LENGTH section_header_margins section_header_margin_count)
if(NOT section_header_margin_count EQUAL section_header_count)
    message(FATAL_ERROR "Every Compositor section header must have a 12 px bottom margin")
endif()

string(FIND "${compositor_source}" "Layout.topMargin:" top_margin_position)
if(NOT top_margin_position EQUAL -1)
    message(FATAL_ERROR "Compositor.qml must not use section-header top margins")
endif()

string(REGEX MATCHALL "Form\\.FormCard[ \\t]*\\{" form_cards "${compositor_source}")
list(LENGTH form_cards form_card_count)
if(NOT form_card_count EQUAL 4)
    message(FATAL_ERROR "Compositor.qml must keep four form cards")
endif()

string(REGEX MATCHALL "Layout\\.bottomMargin: 24" intermediate_card_margins "${compositor_source}")
list(LENGTH intermediate_card_margins intermediate_card_margin_count)
if(NOT intermediate_card_margin_count EQUAL 3)
    message(FATAL_ERROR "Only the three intermediate Compositor cards must have a 24 px bottom margin")
endif()
