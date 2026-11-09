import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import ".." as Components
import "." as Local

// SelectButton for long lists: the same button and rows, with a search field in the
// popup. The owner reacts to `searchEdited` and supplies matching rows through
// `options`. Rows are { id, name, subtitle?, trailing?, heading? }.
Item {
    id: sel

    property string label: ""
    property var options: []
    property string currentId: ""
    property string accessibleName: label
    property string searchPlaceholder: ""
    property string emptyText: ""
    property string searchText: ""
    property int popupWidth: 380
    property int popupHeight: 420
    readonly property bool popupOpen: dropdown.visible
    // Single-letter headings double as the A-Z index beside the list.
    readonly property var sectionIndex: {
        const entries = []
        sel.options.forEach((row, index) => {
            if (row.heading && row.heading.length === 1)
                entries.push({ letter: row.heading, index: index })
        })
        return entries
    }

    signal selected(string id)
    signal searchEdited(string text)
    signal closed()

    readonly property bool isLight: Components.Theme.themeMode === 1
    readonly property color popupHoverBg: isLight ? Qt.rgba(0, 0, 0, 0.055) : Qt.rgba(1, 1, 1, 0.08)
    readonly property color popupBorder: isLight ? Qt.rgba(0, 0, 0, 0.12) : Qt.rgba(1, 1, 1, 0.10)
    readonly property color popupShadow: isLight ? Qt.rgba(0, 0, 0, 0.22) : Qt.rgba(0, 0, 0, 0.73)
    readonly property var labelStyle: Components.Theme.typography.role("label")
    readonly property var captionStyle: Components.Theme.typography.role("caption")

    implicitWidth: 240
    implicitHeight: Math.max(36, Math.ceil(labelStyle.pixelSize * labelStyle.lineHeight) + 16)

    function open() { dropdown.open() }
    function close() { dropdown.close() }
    function indexOfCurrent() {
        for (let i = 0; i < sel.options.length; ++i) {
            if (sel.options[i].id === sel.currentId)
                return i
        }
        return -1
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
        radius: Components.Theme.controlRadius
        color: sel.enabled && buttonArea.containsMouse ? Components.Theme.controlHoverFill : Components.Theme.cardBg
        border.width: 1
        border.color: sel.enabled && (dropdown.visible || buttonArea.pressed || sel.activeFocus)
            ? Components.Theme.accent : Components.Theme.cardBorder

        Behavior on color { ColorAnimation { duration: Components.Theme.animationNormal } }
        Behavior on border.color { ColorAnimation { duration: Components.Theme.animationNormal } }

        PressTint {
            anchors.fill: parent
            radius: parent.radius
            pressed: sel.enabled && buttonArea.pressed
        }

        RowLayout {
            anchors { fill: parent; leftMargin: Components.Theme.spacingMedium; rightMargin: Components.Theme.spacingMedium }
            spacing: Components.Theme.spacing

            Text {
                Layout.fillWidth: true
                text: sel.label
                color: sel.enabled ? Components.Theme.textPrimary : Components.Theme.textSecondary
                font.family: sel.labelStyle.family
                font.pixelSize: sel.labelStyle.pixelSize
                font.weight: Components.Theme.fontWeightMedium
                elide: Text.ElideRight
            }
            Text {
                visible: sel.enabled
                text: "⌄"
                color: dropdown.visible ? Components.Theme.accent : Components.Theme.textSecondary
                font.pixelSize: 13
                rotation: dropdown.visible ? 180 : 0
                Behavior on rotation { NumberAnimation { duration: Components.Theme.animationNormal; easing.type: Components.Theme.easingMove } }
            }
        }

        MouseArea {
            id: buttonArea
            objectName: "searchableSelectMouseArea"
            anchors.fill: parent
            enabled: sel.enabled
            hoverEnabled: sel.enabled
            cursorShape: sel.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: dropdown.visible ? dropdown.close() : dropdown.open()
        }
    }

    Popup {
        id: dropdown
        objectName: "searchableSelectPopup"
        x: sel.width - width
        y: sel.height + 6
        width: Math.max(sel.width, sel.popupWidth)
        height: sel.popupHeight
        margins: Components.Theme.spacingMedium
        padding: Components.Theme.spacingSmall
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
                    shadowBlur: 1.5
                    shadowVerticalOffset: 6
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
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Components.Theme.animationNormal; easing.type: Components.Theme.easingStandard }
                NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Components.Theme.animationNormal; easing.type: Components.Theme.easingEnter }
            }
        }
        exit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Components.Theme.animationFast; easing.type: Components.Theme.easingExit }
                NumberAnimation { property: "scale"; from: 1; to: 0.95; duration: Components.Theme.animationFast; easing.type: Components.Theme.easingExit }
            }
        }

        onAboutToShow: {
            list.currentIndex = Math.max(0, sel.indexOfCurrent())
            list.positionViewAtIndex(list.currentIndex, ListView.Center)
        }
        onOpened: search.focusField(true)
        onClosed: {
            sel.closed()
            sel.forceActiveFocus()
        }

        contentItem: FocusScope {
            focus: true
            Keys.onUpPressed: list.moveHighlight(-1)
            Keys.onDownPressed: list.moveHighlight(1)

            ColumnLayout {
                anchors.fill: parent
                spacing: Components.Theme.spacingSmall

                // Qualified: Qt Quick Controls has its own SearchField.
                Local.SearchField {
                    id: search
                    objectName: "searchableSelectField"
                    Layout.fillWidth: true
                    controlHeight: 36
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

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    ListView {
                        id: list
                        objectName: "searchableSelectList"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: sel.options
                        spacing: Components.Theme.spacingMicro
                        boundsBehavior: Flickable.StopAtBounds
                        highlightMoveDuration: 0
                        keyNavigationEnabled: false
                        ScrollBar.vertical: ScrollBar {
                            id: listScrollBar
                            objectName: "searchableSelectScrollbar"
                            policy: ScrollBar.AsNeeded
                        }
                        // Rows stop short of the bar so it never sits on a highlighted row.
                        readonly property real scrollGutter: contentHeight > height
                            ? listScrollBar.implicitWidth + Components.Theme.spacingMicro : 0

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
                            width: list.width - list.scrollGutter

                            Text {
                                visible: !!row.modelData.heading
                                width: parent.width
                                leftPadding: Components.Theme.spacingMedium
                                topPadding: row.index === 0 ? Components.Theme.spacingTiny : Components.Theme.spacingMedium
                                bottomPadding: Components.Theme.spacingTiny
                                text: row.modelData.heading || ""
                                color: Components.Theme.textTertiary
                                font.family: sel.captionStyle.family
                                font.pixelSize: sel.captionStyle.pixelSize
                                font.weight: Components.Theme.fontWeightDemiBold
                            }

                            Rectangle {
                                width: parent.width
                                height: row.modelData.subtitle ? 50 : 36
                                radius: Components.Theme.cornerRadiusSmall
                                color: rowArea.pressed
                                    ? Qt.rgba(Components.Theme.accent.r, Components.Theme.accent.g, Components.Theme.accent.b, 0.25)
                                    : (row.active ? Qt.rgba(Components.Theme.accent.r, Components.Theme.accent.g, Components.Theme.accent.b, 0.15)
                                                  : (rowArea.containsMouse || row.highlighted ? sel.popupHoverBg : "transparent"))

                                RowLayout {
                                    anchors { fill: parent; leftMargin: Components.Theme.spacingMedium; rightMargin: Components.Theme.spacingMedium }
                                    spacing: Components.Theme.spacing

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        spacing: 0
                                        Text {
                                            Layout.fillWidth: true
                                            text: row.modelData.name
                                            color: row.active ? Components.Theme.accent : Components.Theme.textPrimary
                                            font.family: sel.labelStyle.family
                                            font.pixelSize: sel.labelStyle.pixelSize
                                            font.weight: row.active ? Components.Theme.fontWeightDemiBold : Components.Theme.fontWeightMedium
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            visible: !!row.modelData.subtitle
                                            text: row.modelData.subtitle || ""
                                            // Native names may be right-to-left; keep the column aligned.
                                            horizontalAlignment: Text.AlignLeft
                                            color: Components.Theme.textSecondary
                                            font.family: sel.captionStyle.family
                                            font.pixelSize: sel.captionStyle.pixelSize
                                            elide: Text.ElideRight
                                        }
                                    }
                                    Text {
                                        visible: !!row.modelData.trailing
                                        text: row.modelData.trailing || ""
                                        color: Components.Theme.textTertiary
                                        font.family: sel.captionStyle.family
                                        font.pixelSize: sel.captionStyle.pixelSize
                                        font.features: { "tnum": 1 }
                                    }
                                    Text {
                                        visible: row.active
                                        text: "✓"
                                        color: Components.Theme.accent
                                        font.pixelSize: sel.labelStyle.pixelSize
                                        font.weight: Components.Theme.fontWeightBold
                                    }
                                }

                                MouseArea {
                                    id: rowArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
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

                    // The section index of an Apple list: accent letters beside the
                    // scrollbar. Press or drag to jump; letters thin out if space runs out.
                    Item {
                        id: letterIndex
                        objectName: "searchableSelectIndex"
                        visible: sel.sectionIndex.length > 1
                        Layout.preferredWidth: 18
                        Layout.fillHeight: true
                        Accessible.role: Accessible.List

                        readonly property int step: Math.max(1, Math.ceil(sel.sectionIndex.length * 11 / Math.max(1, height)))
                        readonly property var shown: sel.sectionIndex.filter((_, index) => index % step === 0)
                        readonly property real letterHeight: Math.min(14, height / Math.max(1, shown.length))
                        readonly property real blockHeight: letterHeight * shown.length
                        property string pressedLetter: ""

                        function jump(y) {
                            const offset = (height - blockHeight) / 2
                            const ratio = Math.max(0, Math.min(0.999, (y - offset) / Math.max(1, blockHeight)))
                            const entry = sel.sectionIndex[Math.floor(ratio * sel.sectionIndex.length)]
                            pressedLetter = entry.letter
                            list.currentIndex = entry.index
                            list.positionViewAtIndex(entry.index, ListView.Beginning)
                        }

                        Column {
                            anchors.centerIn: parent
                            Repeater {
                                model: letterIndex.shown
                                Text {
                                    required property var modelData
                                    width: letterIndex.width
                                    height: letterIndex.letterHeight
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    text: modelData.letter
                                    color: letterIndex.pressedLetter === modelData.letter
                                        ? Components.Theme.textPrimary : Components.Theme.accent
                                    font.family: sel.captionStyle.family
                                    font.pixelSize: sel.captionStyle.pixelSize - 2
                                    font.weight: Components.Theme.fontWeightDemiBold
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            preventStealing: true
                            cursorShape: Qt.PointingHandCursor
                            onPressed: mouse => letterIndex.jump(mouse.y)
                            onPositionChanged: mouse => {
                                if (pressed)
                                    letterIndex.jump(mouse.y)
                            }
                            onReleased: letterIndex.pressedLetter = ""
                            onCanceled: letterIndex.pressedLetter = ""
                            // Keep the wheel on the list instead of the page behind the popup.
                            onWheel: wheel => {
                                const last = list.originY + Math.max(0, list.contentHeight - list.height)
                                list.contentY = Math.max(list.originY, Math.min(last, list.contentY - wheel.angleDelta.y))
                            }
                        }
                    }
                }
            }
        }
    }
}
