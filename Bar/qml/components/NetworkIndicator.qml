import QtQuick

TopbarIndicator {
    id: root

    ShellBarTheme { id: theme }

    property var networkService: null
    objectName: "networkIndicator"
    fixedWidth: 28
    height: 34

    readonly property bool netConnected: Boolean(root.networkService
        && root.networkService.available !== false
        && root.networkService.connected)
    readonly property int netType: root.netConnected && root.networkService
        ? root.networkService.connectionType : 0
    readonly property bool wifiAvailable: Boolean(root.networkService && root.networkService.wifiAvailable)

    Text {
        id: icon
        objectName: "networkIcon"
        text: !root.netConnected ? "󰖪"
            : root.netType === 1 ? "󰖩" : "󰈀"
        color: !root.netConnected ? theme.shellIconWarning : theme.shellIconMain
        font { family: theme.iconFontFamily; pixelSize: theme.fontSizeIcon }
        Behavior on color { ColorAnimation { duration: theme.animationFast } }
    }
}
