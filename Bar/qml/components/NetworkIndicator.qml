import QtQuick

Item {
    id: root

    ShellBarTheme { id: theme }

    property var networkService: null
    objectName: "networkIndicator"
    implicitWidth: label.visible ? icon.width + label.width + 6 : icon.width
    implicitHeight: 22
    width: implicitWidth
    height: implicitHeight

    Text {
        id: icon
        objectName: "networkIcon"
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: !root.networkService
            ? "\uf071"
            : root.networkService.connected
                ? root.networkService.connectionType === 1 ? "\uf1eb" : "\uf796"
                : root.networkService.wifiAvailable ? "\uf1eb" : "\uf071"
        color: !root.networkService || (!root.networkService.connected
                                        && !root.networkService.wifiAvailable)
            ? theme.shellIconMuted : theme.shellIconMain
        font.family: "Symbols Nerd Font"
        font.pixelSize: 15
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        id: label
        objectName: "networkLabel"
        anchors.left: icon.right
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        visible: Boolean(root.networkService && root.networkService.connectionName)
        text: root.networkService ? root.networkService.connectionName : ""
        color: theme.shellTextSecondary
        font.family: theme.shellFontFamily
        font.pixelSize: 10
        elide: Text.ElideRight
        width: Math.min(96, implicitWidth)
    }
}
