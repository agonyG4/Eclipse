import QtQuick
import "components"

PopupCard {
    id: root

    ShellBarTheme { id: theme }

    property var bluetoothService: null
    readonly property bool available: Boolean(root.bluetoothService
                                             && root.bluetoothService.available
                                             && root.bluetoothService.adapterAvailable)
    readonly property bool powered: Boolean(root.bluetoothService && root.bluetoothService.powered)
    readonly property bool scanning: Boolean(root.bluetoothService && root.bluetoothService.scanning)
    readonly property bool powerPending: Boolean(root.bluetoothService && root.bluetoothService.powerPending)

    objectName: "bluetoothPopupCard"
    implicitWidth: 280
    cardPadding: 18
    contentSpacing: 8

    onVisibleChanged: {
        if (!root.bluetoothService)
            return
        if (visible && root.powered && root.bluetoothService.requestScan)
            root.bluetoothService.requestScan("bluetooth-popup")
        else if (!visible && root.bluetoothService.releaseScan)
            root.bluetoothService.releaseScan("bluetooth-popup")
    }

    PopupHeader {
        title: "Bluetooth"
        trailingWidth: 44
        trailingHeight: 24

        Rectangle {
            anchors.centerIn: parent
            width: 44
            height: 24
            radius: height / 2
            color: root.powered ? Qt.rgba(0.20, 0.60, 1.0, 0.30)
                                : Qt.rgba(1, 1, 1, 0.07)
            border.width: 1
            border.color: root.powered ? Qt.rgba(0.20, 0.60, 1.0, 0.50)
                                       : Qt.rgba(1, 1, 1, 0.08)
            opacity: root.powerPending ? 0.55 : 1

            Text {
                anchors.centerIn: parent
                text: root.powerPending ? "󰑐" : "󰂯"
                color: root.powered ? theme.shellIconAccent : theme.shellIconMuted
                font { family: theme.iconFontFamily; pixelSize: theme.fontSizeBody }
                RotationAnimation on rotation {
                    running: root.powerPending
                    from: 0
                    to: 360
                    duration: theme.animationPulse
                    loops: Animation.Infinite
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: root.available && !root.powerPending
                cursorShape: Qt.PointingHandCursor
                onClicked: root.bluetoothService.setPowered(!root.powered)
            }
        }
    }

    Rectangle { width: parent.width; height: 1; color: theme.shellSeparator }

    Column {
        width: parent.width
        spacing: 2

        Text {
            visible: !root.available || !root.powered
            width: parent.width
            height: 36
            text: !root.available ? "Bluetooth unavailable" : "Bluetooth off"
            color: theme.shellTextSecondary
            font { family: theme.fontFamily; pixelSize: theme.fontSizeSmall }
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }

        Repeater {
            model: root.bluetoothService && root.powered
                ? root.bluetoothService.devicesModel : null

            delegate: Rectangle {
                required property string name
                required property string objectPath
                required property bool paired
                required property bool connected

                width: root.width
                height: 38
                radius: theme.shellControlRadius
                color: connected ? Qt.rgba(0.20, 0.60, 1.0, 0.12)
                                  : rowMouse.containsMouse ? theme.shellSeparator : "transparent"
                border.width: connected ? 1 : 0
                border.color: Qt.rgba(0.20, 0.60, 1.0, 0.25)

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: theme.spacingMedium
                    anchors.rightMargin: theme.spacingMedium
                    spacing: theme.spacingMedium

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: paired ? "󰂱" : "󰂴"
                        color: connected ? theme.shellIconAccent : theme.shellIconMain
                        font { family: theme.iconFontFamily; pixelSize: theme.fontSizeIcon }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: name || "Bluetooth device"
                        color: connected ? theme.shellTextActive : theme.shellTextSecondary
                        width: parent.width - 28 - theme.spacingMedium
                        elide: Text.ElideRight
                        font {
                            family: theme.fontFamily
                            pixelSize: theme.fontSizeBody
                            weight: connected ? Font.DemiBold : Font.Normal
                        }
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    enabled: paired || connected
                    hoverEnabled: enabled
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: connected
                        ? root.bluetoothService.disconnectDevice(objectPath)
                        : root.bluetoothService.connectDevice(objectPath)
                }
            }
        }
    }

    Rectangle {
        visible: root.powered
        width: parent.width
        height: 32
        radius: theme.shellControlRadius
        color: scanMouse.containsMouse ? theme.shellSeparator : "transparent"
        border.width: root.scanning ? 1 : 0
        border.color: Qt.rgba(0.20, 0.60, 1.0, 0.25)

        Row {
            anchors.centerIn: parent
            spacing: theme.spacingSmall

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.scanning ? "󰑐" : "󰍉"
                color: root.scanning ? theme.shellIconAccent : theme.shellTextSecondary
                font { family: theme.iconFontFamily; pixelSize: theme.fontSizeIcon }
                RotationAnimation on rotation {
                    running: root.scanning
                    from: 0
                    to: 360
                    duration: theme.animationSpin
                    loops: Animation.Infinite
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.scanning ? "Searching…" : "Search for devices"
                color: root.scanning ? theme.shellIconAccent : theme.shellTextDim
                font { family: theme.fontFamily; pixelSize: theme.fontSizeSmall; weight: Font.Medium }
            }
        }

        MouseArea {
            id: scanMouse
            anchors.fill: parent
            enabled: root.available && root.powered && !root.scanning
            hoverEnabled: enabled
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: root.bluetoothService.requestScan("bluetooth-popup")
        }
    }
}
