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

    readonly property bool connected: Boolean(root.networkService && root.networkService.connected)
    readonly property int connectionType: root.networkService
        ? root.networkService.connectionType : 0
    readonly property string connectionTitle: root.connected
        ? (root.networkService.connectionName || (root.connectionType === 1 ? "Wi-Fi" : "Ethernet"))
        : (root.networkService && root.networkService.wifiAvailable ? "Wi-Fi" : "Network")
    readonly property string connectionIcon: root.connectionType === 1 ? "󰖩" : "󰈀"

    PopupHeader {
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
                color: theme.shellIconMuted
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
                color: theme.shellIconMuted
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
