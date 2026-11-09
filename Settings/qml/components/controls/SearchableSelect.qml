import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Shapes
import ".." as Components
import "." as Local

// A macOS pop-up button: plain text and double chevrons, opening a menu whose row
// under the pointer takes the accent and whose chosen row carries a leading check.
// With `searchable`, the owner reacts to `searchEdited` and supplies matching rows
// through `options`. Rows are { id, name, subtitle?, flag?, trailing?, heading? }.
Item {
    id: sel

    property string label: ""
    property string leadingText: ""
    property var options: []
    property string currentId: ""
    property string accessibleName: label
    property bool searchable: true
    property string searchPlaceholder: ""
    property string emptyText: ""
    property string searchText: ""
    property int popupWidth: searchable ? 360 : 240
    property int popupHeight: 400
    property int maximumLabelWidth: 300
    readonly property bool popupOpen: dropdown.visible

    signal selected(string id)
    signal searchEdited(string text)
    signal closed()

    readonly property bool isLight: Components.Theme.themeMode === 1
    readonly property color popupBorder: isLight ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(1, 1, 1, 0.12)
    readonly property color popupShadow: isLight ? Qt.rgba(0, 0, 0, 0.20) : Qt.rgba(0, 0, 0, 0.60)
    readonly property var labelStyle: Components.Theme.typography.role("label")
    readonly property var captionStyle: Components.Theme.typography.role("caption")
    readonly property int menuPadding: 6
    readonly property int singleRow: 30
    readonly property int doubleRow: 44

    implicitWidth: buttonRow.implicitWidth + Components.Theme.spacingMedium * 2
    implicitHeight: 30

    function open() { dropdown.open() }
    function close() { dropdown.close() }
    function indexOfCurrent() {
        for (let i = 0; i < sel.options.length; ++i) {
            if (sel.options[i].id === sel.currentId)
                return i
        }
        return -1
    }
    function listHeight() {
        return sel.options.reduce((sum, row) => sum + (row.subtitle ? sel.doubleRow : sel.singleRow), 0)
    }

    activeFocusOnTab: enabled
    Accessible.role: Accessible.ComboBox
    Accessible.name: sel.accessibleName
    Accessible.description: sel.label

    Keys.onPressed: event => {
        if (sel.enabled && (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
            dropdown.open()
            event.accepted = true
        }
    }

    onSearchTextChanged: if (search.text !== searchText) search.text = searchText

    Rectangle {
        anchors.fill: parent
        radius: Components.Theme.cornerRadiusSmall
        color: sel.enabled && (buttonArea.containsMouse || dropdown.visible)
            ? Components.Theme.controlHoverFill : "transparent"
        border.width: sel.activeFocus ? 2 : 0
        border.color: Qt.rgba(Components.Theme.accent.r, Components.Theme.accent.g, Components.Theme.accent.b, 0.6)
        Behavior on color { ColorAnimation { duration: Components.Theme.animationQuick } }

        RowLayout {
            id: buttonRow
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left; right: parent.right
                leftMargin: Components.Theme.spacingMedium; rightMargin: Components.Theme.spacingMedium
            }
            spacing: Components.Theme.spacingSmall

            Text {
                visible: sel.leadingText !== ""
                text: sel.leadingText
                font.pixelSize: sel.labelStyle.pixelSize + 1
            }
            Text {
                Layout.fillWidth: true
                Layout.maximumWidth: sel.maximumLabelWidth
                text: sel.label
                color: sel.enabled ? Components.Theme.textPrimary : Components.Theme.textTertiary
                horizontalAlignment: Text.AlignRight
                font.family: sel.labelStyle.family
                font.pixelSize: sel.labelStyle.pixelSize
                font.letterSpacing: sel.labelStyle.letterSpacing
                elide: Text.ElideRight
            }
            // The up-and-down chevrons of an NSPopUpButton.
            Shape {
                visible: sel.enabled
                Layout.preferredWidth: 8
                Layout.preferredHeight: 13
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: Components.Theme.textSecondary
                    strokeWidth: 1.4
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: 0.7; startY: 4.4
                    PathLine { x: 4; y: 1 }
                    PathLine { x: 7.3; y: 4.4 }
                    PathMove { x: 0.7; y: 8.6 }
                    PathLine { x: 4; y: 12 }
                    PathLine { x: 7.3; y: 8.6 }
                }
            }
        }

        MouseArea {
            id: buttonArea
            objectName: "searchableSelectMouseArea"
            anchors.fill: parent
            enabled: sel.enabled
            hoverEnabled: sel.enabled
            onClicked: dropdown.visible ? dropdown.close() : dropdown.open()
        }
    }

    Popup {
        id: dropdown
        objectName: "searchableSelectPopup"
        x: sel.width - width
        y: sel.height + 4
        width: Math.max(sel.width, sel.popupWidth)
        height: sel.searchable ? sel.popupHeight
                               : Math.min(sel.popupHeight, sel.listHeight() + sel.menuPadding * 2)
        margins: Components.Theme.spacingMedium
        padding: sel.menuPadding
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        // The shadow has its own layer, so the panel stays opaque wherever the
        // effect can't run: a transparent list over the page is unreadable.
        background: Item {
            Rectangle {
                anchors.fill: parent
                radius: Components.Theme.cornerRadiusLarge
                color: Components.Theme.popupBg
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: sel.popupShadow
                    shadowBlur: 1.2
                    shadowVerticalOffset: 8
                }
            }
            Rectangle {
                anchors.fill: parent
                radius: Components.Theme.cornerRadiusLarge
                color: Components.Theme.popupBg
                border.width: 1
                border.color: sel.popupBorder
            }
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Components.Theme.animationQuick; easing.type: Components.Theme.easingStandard }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Components.Theme.animationNormal; easing.type: Components.Theme.easingExit }
        }

        onAboutToShow: {
            list.currentIndex = Math.max(0, sel.indexOfCurrent())
            list.positionViewAtIndex(list.currentIndex, ListView.Center)
        }
        onOpened: {
            if (sel.searchable)
                search.focusField(true)
            else
                list.forceActiveFocus()
        }
        onClosed: {
            sel.closed()
            sel.forceActiveFocus()
        }

        contentItem: FocusScope {
            focus: true
            Keys.onUpPressed: list.moveHighlight(-1)
            Keys.onDownPressed: list.moveHighlight(1)
            Keys.onReturnPressed: list.chooseCurrent()
            Keys.onEnterPressed: list.chooseCurrent()

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Qualified: Qt Quick Controls has its own SearchField.
                Local.SearchField {
                    id: search
                    objectName: "searchableSelectField"
                    visible: sel.searchable
                    Layout.fillWidth: true
                    controlHeight: 30
                    clearOnEscape: false
                    placeholderText: sel.searchPlaceholder
                    onTextEdited: value => {
                        sel.searchEdited(value)
                        list.currentIndex = 0
                        list.positionViewAtBeginning()
                    }
                    onEscapePressed: dropdown.close()
                    onAccepted: list.chooseCurrent()
                }

                Rectangle {
                    visible: sel.searchable
                    Layout.fillWidth: true
                    Layout.topMargin: sel.menuPadding
                    Layout.leftMargin: -sel.menuPadding
                    Layout.rightMargin: -sel.menuPadding
                    Layout.preferredHeight: 1
                    color: sel.popupBorder
                }

                ListView {
                    id: list
                    objectName: "searchableSelectList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: sel.searchable ? sel.menuPadding : 0
                    clip: true
                    model: sel.options
                    boundsBehavior: Flickable.StopAtBounds
                    highlightMoveDuration: 0
                    keyNavigationEnabled: false
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    function moveHighlight(delta) {
                        if (count === 0)
                            return
                        currentIndex = Math.max(0, Math.min(count - 1, currentIndex + delta))
                        positionViewAtIndex(currentIndex, ListView.Contain)
                    }
                    function chooseCurrent() {
                        if (currentIndex >= 0 && currentIndex < sel.options.length)
                            choose(sel.options[currentIndex].id)
                    }
                    function choose(id) {
                        sel.selected(id)
                        dropdown.close()
                    }

                    delegate: Column {
                        id: row
                        required property var modelData
                        required property int index
                        readonly property bool active: modelData.id === sel.currentId
                        readonly property bool highlighted: ListView.isCurrentItem
                        readonly property color primaryText: highlighted ? Components.Theme.accentForeground : Components.Theme.textPrimary
                        readonly property color secondaryText: highlighted
                            ? Qt.rgba(Components.Theme.accentForeground.r, Components.Theme.accentForeground.g, Components.Theme.accentForeground.b, 0.8)
                            : Components.Theme.textSecondary
                        width: ListView.view.width

                        Text {
                            visible: !!row.modelData.heading
                            width: parent.width
                            leftPadding: Components.Theme.spacingSmall
                            topPadding: row.index === 0 ? Components.Theme.spacingTiny : Components.Theme.spacingMedium
                            bottomPadding: Components.Theme.spacingTiny
                            text: row.modelData.heading || ""
                            color: Components.Theme.textSecondary
                            font.family: sel.captionStyle.family
                            font.pixelSize: sel.captionStyle.pixelSize
                            font.weight: Components.Theme.fontWeightDemiBold
                        }

                        Rectangle {
                            width: parent.width
                            height: row.modelData.subtitle ? sel.doubleRow : sel.singleRow
                            radius: Components.Theme.cornerRadiusSmall
                            color: row.highlighted ? Components.Theme.accent : "transparent"

                            RowLayout {
                                anchors { fill: parent; leftMargin: Components.Theme.spacingSmall; rightMargin: Components.Theme.spacingMedium }
                                spacing: Components.Theme.spacingSmall

                                Text {
                                    Layout.preferredWidth: 14
                                    text: row.active ? "✓" : ""
                                    color: row.primaryText
                                    font.pixelSize: sel.labelStyle.pixelSize - 1
                                    font.weight: Components.Theme.fontWeightDemiBold
                                }
                                Text {
                                    visible: !!row.modelData.flag
                                    text: row.modelData.flag || ""
                                    font.pixelSize: sel.labelStyle.pixelSize + 2
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 0
                                    Text {
                                        Layout.fillWidth: true
                                        text: row.modelData.name
                                        color: row.primaryText
                                        font.family: sel.labelStyle.family
                                        font.pixelSize: sel.labelStyle.pixelSize
                                        font.letterSpacing: sel.labelStyle.letterSpacing
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        visible: !!row.modelData.subtitle
                                        text: row.modelData.subtitle || ""
                                        color: row.secondaryText
                                        font.family: sel.captionStyle.family
                                        font.pixelSize: sel.captionStyle.pixelSize
                                        elide: Text.ElideRight
                                    }
                                }
                                Text {
                                    visible: !!row.modelData.trailing
                                    text: row.modelData.trailing || ""
                                    color: row.secondaryText
                                    font.family: sel.captionStyle.family
                                    font.pixelSize: sel.captionStyle.pixelSize
                                    font.features: { "tnum": 1 }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: list.currentIndex = row.index
                                onClicked: list.choose(row.modelData.id)
                            }
                        }
                    }

                    Text {
                        objectName: "searchableSelectEmpty"
                        anchors.centerIn: parent
                        width: parent.width - Components.Theme.spacingXLarge * 2
                        visible: list.count === 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: sel.emptyText
                        color: Components.Theme.textSecondary
                        font.family: sel.labelStyle.family
                        font.pixelSize: sel.labelStyle.pixelSize
                    }
                }
            }
        }
    }
}
