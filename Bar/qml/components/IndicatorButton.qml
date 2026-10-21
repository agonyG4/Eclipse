import QtQuick

Item {
    id: control

    ShellBarTheme { id: theme }

    property bool active: false
    property bool interactive: true
    property bool reorderable: false
    property var statusLayout: null
    property string orderKey: ""
    property Item reorderItem: control
    property string mouseAreaObjectName: ""
    property string backgroundObjectName: ""
    property int acceptedButtons: Qt.LeftButton | Qt.RightButton
    property int fixedWidth: 0
    property int horizontalPadding: theme.spacingContainer
    property int backgroundMargin: theme.spacingTiny
    property real backgroundRadius: theme.shellRadiusMedium - 2
    property color activeColor: theme.shellActive
    property color pressedColor: theme.shellPressed
    property color hoverColor: theme.shellHover
    property color idleColor: "transparent"
    property alias spacing: contentRow.spacing
    readonly property bool hovered: hoverHandler.hovered
    readonly property bool pressed: mouseArea.pressed
    readonly property bool dragging: mouseArea.dragging

    default property alias contentData: contentRow.data

    signal clicked(real anchorX)
    signal buttonClicked(int button, real anchorX)
    signal wheel(var event)
    signal reorderStarted()

    width: fixedWidth > 0 ? fixedWidth : contentRow.implicitWidth + horizontalPadding
    height: 34

    Rectangle {
        objectName: control.backgroundObjectName
        anchors.fill: parent
        anchors.margins: control.backgroundMargin
        radius: control.backgroundRadius
        color: control.active ? control.activeColor
             : control.pressed ? control.pressedColor
             : control.hovered ? control.hoverColor
             : control.idleColor

        Behavior on color { ColorAnimation { duration: theme.animationMicro } }
    }

    HoverHandler { id: hoverHandler }

    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: theme.spacingMicro
    }

    MouseArea {
        id: mouseArea
        objectName: control.mouseAreaObjectName
        anchors.fill: parent
        property bool holdConsumed: false
        property bool dragging: false
        property bool dragThresholdCrossed: false
        property real pressSceneX: 0
        property real pressSceneY: 0
        enabled: control.interactive || control.reorderable
        acceptedButtons: control.acceptedButtons
        hoverEnabled: control.interactive || control.reorderable
        cursorShape: control.reorderable && dragging ? Qt.ClosedHandCursor
            : control.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor

        Timer {
            id: holdTimer
            interval: control.statusLayout ? control.statusLayout.holdInterval : 500
            repeat: false
            onTriggered: {
                if (mouseArea.pressed && control.reorderable && control.statusLayout) {
                    mouseArea.holdConsumed = true
                    const item = control.reorderItem || control
                    mouseArea.dragging = control.statusLayout.beginDrag(item, control.orderKey)
                    if (mouseArea.dragging)
                        control.reorderStarted()
                }
            }
        }

        onPressed: mouse => {
            holdConsumed = false
            dragging = false
            dragThresholdCrossed = false
            if (mouse.button === Qt.LeftButton && control.reorderable && control.statusLayout) {
                const point = control.mapToItem(null, mouse.x, mouse.y)
                pressSceneX = point.x
                pressSceneY = point.y
                holdTimer.start()
            }
        }

        onPositionChanged: mouse => {
            if (!pressed)
                return
            const point = control.mapToItem(null, mouse.x, mouse.y)
            const holdMoveThreshold = control.statusLayout
                ? control.statusLayout.dragThreshold : 8
            if (holdTimer.running && (Math.abs(point.x - pressSceneX) >= holdMoveThreshold
                                      || Math.abs(point.y - pressSceneY) >= holdMoveThreshold))
                holdTimer.stop()
            if (!dragging)
                return
            const threshold = control.statusLayout ? control.statusLayout.dragThreshold : 8
            if (!dragThresholdCrossed && Math.abs(point.x - pressSceneX) >= threshold)
                dragThresholdCrossed = true
            if (dragThresholdCrossed) {
                const item = control.reorderItem || control
                control.statusLayout.updateDrag(item, control.orderKey, point.x)
            }
        }

        onReleased: {
            holdTimer.stop()
            if (dragging) {
                const item = control.reorderItem || control
                control.statusLayout.endDrag(item, control.orderKey, true)
                dragging = false
                holdConsumed = true
            }
            dragThresholdCrossed = false
        }

        onCanceled: {
            holdTimer.stop()
            if (dragging && control.statusLayout) {
                const item = control.reorderItem || control
                control.statusLayout.endDrag(item, control.orderKey, false)
            }
            dragging = false
            holdConsumed = true
            dragThresholdCrossed = false
        }

        onClicked: mouse => {
            if (holdConsumed)
                return
            const point = control.mapToItem(null, control.width / 2, control.height / 2)
            control.clicked(point.x)
            control.buttonClicked(mouse.button, point.x)
        }

        onWheel: event => control.wheel(event)
    }
}
