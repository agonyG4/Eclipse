import QtQuick
import "components"

PopupCard {
    id: root

    ShellBarTheme { id: theme }

    property var bluetoothService: null
    readonly property bool available: Boolean(root.bluetoothService
                                             && root.bluetoothService.available
                                             && root.bluetoothService.adapterAvailable)
    readonly property bool powered: root.available
        && Boolean(root.bluetoothService && root.bluetoothService.powered)
    readonly property bool scanning: root.available
        && Boolean(root.bluetoothService && root.bluetoothService.scanning)
    readonly property bool powerPending: Boolean(root.bluetoothService && root.bluetoothService.powerPending)
    readonly property var devicesModel: root.bluetoothService && root.powered
        ? root.bluetoothService.devicesModel : null

    objectName: "bluetoothPopupCard"
    implicitWidth: 280
    cardPadding: 18
    contentSpacing: 14

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

    Text {
        id: pairedSection
        objectName: "pairedSection"
        visible: root.powered && pairedRows.height > 0
        width: parent.width
        text: "Paired devices"
        color: theme.shellTextSecondary
        font { family: theme.fontFamily; pixelSize: theme.fontSizeCaption; weight: Font.DemiBold }
        bottomPadding: 2
    }

    Column {
        id: pairedRows
        objectName: "pairedRows"
        width: parent.width
        spacing: 2
        height: childrenRect.height

        Repeater {
            model: root.devicesModel
            delegate: BluetoothDeviceRow { showPaired: true }
        }
    }

    Text {
        visible: root.powered && pairedRows.height === 0 && availableRows.height === 0
        width: parent.width
        height: 36
        text: "No paired devices"
        color: theme.shellTextSecondary
        font { family: theme.fontFamily; pixelSize: theme.fontSizeSmall }
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }

    Rectangle {
        id: deviceScanSeparator
        objectName: "deviceScanSeparator"
        visible: root.powered
        width: parent.width
        height: 1
        color: theme.shellSeparator
    }

    Rectangle {
        id: scanAction
        objectName: "scanAction"
        visible: root.powered
        width: parent.width
        height: 32
        radius: theme.shellControlRadius
        color: root.scanning
            ? Qt.rgba(0.20, 0.60, 1.0, 0.10)
            : scanMouse.containsMouse ? theme.shellSeparator : "transparent"
        border.width: root.scanning ? 1 : 0
        border.color: Qt.rgba(0.20, 0.60, 1.0, 0.25)
        Behavior on color { ColorAnimation { duration: theme.animationFast } }

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

    Text {
        id: availableSection
        objectName: "availableSection"
        visible: root.powered && availableRows.height > 0
        width: parent.width
        text: "Available devices"
        color: theme.shellTextSecondary
        font { family: theme.fontFamily; pixelSize: theme.fontSizeCaption; weight: Font.DemiBold }
        bottomPadding: 2
    }

    Column {
        id: availableRows
        objectName: "availableRows"
        width: parent.width
        spacing: 2
        height: childrenRect.height

        Repeater {
            model: root.devicesModel
            delegate: BluetoothDeviceRow { showPaired: false }
        }
    }

    component BluetoothDeviceRow: Rectangle {
        id: deviceRow
        required property string name
        required property string objectPath
        required property bool paired
        required property bool connected
        required property int rssi
        required property int batteryPercent
        property bool showPaired: true

        objectName: showPaired ? "pairedDeviceRow" : "availableDeviceRow"
        width: root.width
        height: (paired === showPaired) ? 38 : 0
        visible: paired === showPaired
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
                    - (deviceStatus.visible
                        ? deviceStatus.implicitWidth + theme.spacingMedium : 0)
                elide: Text.ElideRight
                font {
                    family: theme.fontFamily
                    pixelSize: theme.fontSizeBody
                    weight: connected ? Font.DemiBold : Font.Normal
                }
            }

            Item {
                id: deviceStatusSlot
                width: deviceStatus.visible ? deviceStatus.implicitWidth : 0
                height: parent.height

                Text {
                    id: deviceStatus
                    anchors.centerIn: parent
                    visible: batteryPercent >= 0 || rssi !== -1
                    text: batteryPercent >= 0
                        ? batteryPercent + "%"
                          + (rssi !== -1 ? " · " + rssi + " dBm" : "")
                        : rssi + " dBm"
                    color: connected ? theme.shellTextLight : theme.shellTextDim
                    font { family: theme.fontFamily; pixelSize: theme.fontSizeCaption }
                }
            }
        }

        MouseArea {
            id: rowMouse
            objectName: "deviceMouse"
            anchors.fill: parent
            enabled: paired
            hoverEnabled: enabled
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: connected
                ? root.bluetoothService.disconnectDevice(objectPath)
                : root.bluetoothService.connectDevice(objectPath)
        }
    }
}
