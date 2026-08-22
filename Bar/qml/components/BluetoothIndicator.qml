import QtQuick

TopbarIndicator {
    id: root

    ShellBarTheme { id: theme }

    property var bluetoothService: null
    objectName: "bluetoothIndicator"
    fixedWidth: 28
    height: 34
    spacing: 5

    readonly property bool btAvailable: Boolean(root.bluetoothService
                                                 && root.bluetoothService.available
                                                 && root.bluetoothService.adapterAvailable)
    readonly property bool btOn: root.btAvailable
        && Boolean(root.bluetoothService && root.bluetoothService.powered)
    readonly property int connectedCount: root.btOn && root.bluetoothService
        ? root.bluetoothService.connectedCount : 0
    readonly property bool scanning: root.btAvailable
        && Boolean(root.bluetoothService && root.bluetoothService.scanning)
    readonly property int scanPulseAnimationDuration: theme.animationPulse
    readonly property int scanPulseScaleEasing: Easing.OutCubic

        Item {
            width: 16
            height: 16

        Rectangle {
            id: scanPulse
            objectName: "scanPulse"
            anchors.centerIn: parent
            width: 16
            height: 16
            radius: 8
            color: "transparent"
            border.width: 1.5
            border.color: Qt.rgba(0.35, 0.65, 1, 0.7)
            visible: root.scanning

            SequentialAnimation on opacity {
                objectName: "scanPulseOpacityAnimation"
                running: root.scanning
                loops: Animation.Infinite
                NumberAnimation { to: 0; duration: theme.animationPulse }
                NumberAnimation { to: 0.9; duration: 0 }
            }
            SequentialAnimation on scale {
                objectName: "scanPulseScaleAnimation"
                running: root.scanning
                loops: Animation.Infinite
                NumberAnimation { to: 1.8; duration: theme.animationPulse; easing.type: Easing.OutCubic }
                NumberAnimation { to: 1.0; duration: 0 }
            }
        }

        Text {
            id: icon
            objectName: "bluetoothIcon"
            text: root.btOn ? "󰂯" : "󰂲"
            color: !root.btAvailable || !root.btOn
            ? theme.shellIconMuted
            : root.connectedCount > 0
                ? theme.shellIconAccent : theme.shellIconMain
            font { family: theme.iconFontFamily; pixelSize: theme.fontSizeIcon }
            Behavior on color { ColorAnimation { duration: theme.animationFast } }
        }
    }
}
