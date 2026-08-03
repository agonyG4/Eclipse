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

string(REGEX MATCH "(^|\n)Item[ \t]*\{\n    id: root" root_structure "${compositor_source}")
if(root_structure STREQUAL "")
    message(FATAL_ERROR "Compositor.qml must use an Item root")
endif()

string(REGEX MATCHALL "(^|\n)    Form\\.ScrollPage[ \t]*\\{" scroll_pages "${compositor_source}")
list(LENGTH scroll_pages scroll_page_count)
if(NOT scroll_page_count EQUAL 1)
    message(FATAL_ERROR "Compositor.qml must contain one direct Form.ScrollPage")
endif()

string(REGEX MATCH "(^|\n)    Form\\.ScrollPage[ \t]*\\{\n        anchors\\.fill: parent\n        contentMargins: 32\n        maxWidth: 900" page_structure "${compositor_source}")
if(page_structure STREQUAL "")
    message(FATAL_ERROR "Compositor.qml must match the canonical ScrollPage composition")
endif()

string(FIND "${compositor_source}" "ColumnLayout {" wrapper_layout_position)
if(NOT wrapper_layout_position EQUAL -1)
    message(FATAL_ERROR "Compositor.qml must not wrap sections in a ColumnLayout")
endif()

string(FIND "${compositor_source}" "Layout.topMargin:" top_margin_position)
if(NOT top_margin_position EQUAL -1)
    message(FATAL_ERROR "Compositor.qml must not use section-header top margins")
endif()

string(REGEX MATCHALL "Form\\.SectionHeader[ \\t]*\\{" all_section_headers "${compositor_source}")
string(REGEX MATCHALL "(^|\n)        Form\\.SectionHeader[ \t]*\\{" direct_section_headers "${compositor_source}")
list(LENGTH all_section_headers all_section_header_count)
list(LENGTH direct_section_headers direct_section_header_count)
if(NOT all_section_header_count EQUAL 4 OR NOT direct_section_header_count EQUAL 4)
    message(FATAL_ERROR "Compositor.qml must contain four direct section headers")
endif()

string(REGEX MATCHALL "(^|\n)        Form\\.SectionHeader[ \t]*\\{\n            text:[^\n]*\n            Layout\\.bottomMargin: 12\n        \\}" section_header_blocks "${compositor_source}")
list(LENGTH section_header_blocks section_header_block_count)
if(NOT section_header_block_count EQUAL 4)
    message(FATAL_ERROR "Every direct Compositor section header must have a 12 px bottom margin")
endif()

string(REGEX MATCHALL "Form\\.FormCard[ \\t]*\\{" all_form_cards "${compositor_source}")
string(REGEX MATCHALL "(^|\n)        Form\\.FormCard[ \t]*\\{" direct_form_cards "${compositor_source}")
list(LENGTH all_form_cards all_form_card_count)
list(LENGTH direct_form_cards direct_form_card_count)
if(NOT all_form_card_count EQUAL 4 OR NOT direct_form_card_count EQUAL 4)
    message(FATAL_ERROR "Compositor.qml must contain four direct form cards")
endif()

string(REGEX MATCHALL "(^|\n)        Form\\.FormCard[ \t]*\\{\n            Layout\\.bottomMargin: 24" intermediate_card_blocks "${compositor_source}")
list(LENGTH intermediate_card_blocks intermediate_card_block_count)
if(NOT intermediate_card_block_count EQUAL 3)
    message(FATAL_ERROR "Three intermediate Compositor cards must have a 24 px bottom margin")
endif()

string(REGEX MATCHALL "(^|\n)        Form\\.FormCard[ \t]*\\{\n            Layout\\.bottomMargin: 28" final_card_blocks "${compositor_source}")
list(LENGTH final_card_blocks final_card_block_count)
if(NOT final_card_block_count EQUAL 1)
    message(FATAL_ERROR "The final Compositor card must have a 28 px bottom margin")
endif()
