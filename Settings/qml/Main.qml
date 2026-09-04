import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Controls
import QtQuick.Window
import "components/controls" as Controls

ApplicationWindow {
    id: window

    title: I18n.tr("settings.title", "Astrea Settings")
    visible: true
    readonly property int defaultWidth: 1050
    width: defaultWidth
    height: Math.min(760, Screen.desktopAvailableHeight - 32)
    minimumWidth: 800
    minimumHeight: 650
    maximumWidth: 1400
    maximumHeight: Screen.desktopAvailableHeight - 16
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeNormal
    font.weight: Theme.fontWeightNormal
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    background: Rectangle { color: "transparent" }

    readonly property color accent: Theme.accent
    readonly property color textPrimary: Theme.textPrimary
    readonly property color textSecondary: Theme.textSecondary
    readonly property url selectedPageSource: SettingsController.selectedPageSource

    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        radius: 0
        color: "transparent"
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.themeMode === 1 ? Qt.rgba(0, 0, 0, 0.24) : Qt.rgba(0, 0, 0, 0.6)
            shadowBlur: 1.0
            shadowVerticalOffset: 8
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 0
        color: Theme.windowBackground
        border.width: 1
        border.color: Theme.windowBorder
        clip: false

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Theme.windowWash
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 1
            border.color: Theme.themeMode === 1
                ? Qt.rgba(1, 1, 1, Theme.shellStyle === 0 ? 0.22 : Theme.shellStyle === 2 ? 0.30 : 0.14)
                : Qt.rgba(1, 1, 1, Theme.shellStyle === 0 ? 0.04 : 0.02)
        }

        MouseArea {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 14
            cursorShape: Qt.SizeAllCursor
            onPressed: window.startSystemMove()
        }

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Sidebar {
                Layout.preferredWidth: 256
                Layout.fillHeight: true
                model: SettingsController.navigationModel
                selectedId: SettingsController.selectedSidebarId
                translationMessages: I18n.messages
                userName: SettingsController.userName
                avatarPath: SettingsController.avatarUrl
                isSudo: SettingsController.isSudo
                iconTheme: Theme.iconTheme
                iconUrlResolver: (iconKey, iconTheme) => SettingsController.iconUrl(iconKey, iconTheme)
                onSelectId: id => SettingsController.navigateTo(id)
            }

            Item {
                id: contentPane
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Item {
                        id: navigationToolbar
                        objectName: "settingsNavigationToolbar"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52

                        Controls.DualButton {
                            id: backForwardControl
                            objectName: "settingsBackForwardControl"
                            anchors.left: parent.left
                            anchors.leftMargin: 24
                            anchors.verticalCenter: parent.verticalCenter
                            controlWidth: 68
                            controlHeight: 30
                            segmentWidth: 34
                            iconSize: 16
                            iconFontFamily: "JetBrainsMono Nerd Font"
                            leftIconText: "\uf060"
                            rightIconText: "\uf061"
                            leftEnabled: SettingsController.canGoBack
                            rightEnabled: SettingsController.canGoForward
                            onLeftClicked: SettingsController.goBack()
                            onRightClicked: SettingsController.goForward()
                        }
                    }

                    Loader {
                        id: pageLoader
                        objectName: "settingsPageLoader"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: window.selectedPageSource.toString() !== ""
                        source: window.selectedPageSource
                    }
                }
            }
        }
    }
}
