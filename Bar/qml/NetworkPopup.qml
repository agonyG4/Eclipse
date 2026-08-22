import QtQuick
import "components"

PopupCard {
    id: root

    ShellBarTheme { id: theme }

    property var networkService: null

    objectName: "networkPopupCard"
    implicitWidth: 280
    cardPadding: 18
    contentSpacing: 14

    readonly property int stateUnavailable: 0
    readonly property int stateDisconnected: 1
    readonly property int stateWifi: 2
    readonly property int stateWired: 3
    readonly property int stateOther: 4
    readonly property bool serviceAvailable: Boolean(root.networkService
        && root.networkService.available !== false)
    readonly property bool connected: root.serviceAvailable
        && Boolean(root.networkService && root.networkService.connected)
    readonly property int connectionType: root.networkService
        ? root.networkService.connectionType : 0
    readonly property int connectionState: !root.serviceAvailable
        ? root.stateUnavailable
        : !root.connected
            ? root.stateDisconnected
            : root.connectionType === 1
                ? root.stateWifi
                : root.connectionType === 2
                    ? root.stateWired : root.stateOther
    readonly property string connectionTitle: root.connectionState === root.stateWifi
        ? (root.networkService.connectionName || "Wi-Fi")
        : root.connectionState === root.stateWired
            ? (root.networkService.connectionName || "Ethernet")
            : root.connectionState === root.stateOther
                ? (root.networkService.connectionName || "Network") : "Network"
    readonly property string connectionIcon: root.connectionState === root.stateWifi
        ? "󰖩"
        : root.connectionState === root.stateWired ? "󰈀" : "󰖪"
    readonly property string connectionStateLabel: root.connectionState === root.stateUnavailable
        ? "Unavailable"
        : root.connectionState === root.stateDisconnected
            ? "Disconnected" : "Connected"

    PopupHeader {
        objectName: "networkPopupHeader"
        title: root.connectionTitle
        icon: root.connectionIcon
    }

    Row {
        width: parent.width
        spacing: theme.spacingXXLarge

        Row {
            spacing: theme.spacing
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "󰇚"
                color: Qt.rgba(1, 1, 1, 0.40)
                font { family: theme.iconFontFamily; pixelSize: theme.fontSizeIconLarge }
            }
            Column {
                Text {
                    text: "Download"
                    color: theme.shellTextSecondary
                    font { family: theme.fontFamily; pixelSize: theme.fontSizeExtraSmall }
                }
                Text {
                    text: root.networkService && root.networkService.downloadRate
                        ? root.networkService.downloadRate : "0 B/s"
                    color: theme.shellTextActive
                    opacity: theme.opacitySecondary
                    font { family: theme.fontFamily; pixelSize: theme.fontSizeBody; weight: Font.Medium }
                }
            }
        }

        Row {
            spacing: theme.spacing
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "󰕒"
                color: Qt.rgba(1, 1, 1, 0.40)
                font { family: theme.iconFontFamily; pixelSize: theme.fontSizeIconLarge }
            }
            Column {
                Text {
                    text: "Upload"
                    color: theme.shellTextSecondary
                    font { family: theme.fontFamily; pixelSize: theme.fontSizeExtraSmall }
                }
                Text {
                    text: root.networkService && root.networkService.uploadRate
                        ? root.networkService.uploadRate : "0 B/s"
                    color: theme.shellTextActive
                    opacity: theme.opacitySecondary
                    font { family: theme.fontFamily; pixelSize: theme.fontSizeBody; weight: Font.Medium }
                }
            }
        }
    }
}
