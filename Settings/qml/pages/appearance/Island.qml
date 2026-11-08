import QtQuick
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "islandPage"

    property bool islandEnabled: true
    property bool alwaysOnTop: true
    property int style: 1
    property bool musicEnabled: true
    property bool gamemodeNotifications: true

    readonly property var styles: ["Notch", "Bubble"]
    readonly property real islandRadius: [10, 28][root.style]

    Form.ScrollPage {
        objectName: "islandScrollPage"
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Form.PageHeader {
            title: I18n.tr("settings.nav.island", "Island")
            subtitle: I18n.tr("apps.settings.pages.island.header_subtitle", "Customize the desktop island.")
        }

        Components.BackendNotice {
            objectName: "islandPreviewNotice"
            message: I18n.tr("apps.settings.pages.island.subtitle", "Preview only: changes here are not saved yet.")
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.island.preview", "PREVIEW")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "islandLivePreview"
            Layout.bottomMargin: 24

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 128

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(parent.width - 40, 250)
                    height: root.islandEnabled ? 78 : 50
                    radius: root.islandRadius
                    color: Components.Theme.islandBackground
                    border.width: 1
                    border.color: Components.Theme.shellBorder
                    opacity: root.islandEnabled ? 1 : 0.5
                    Behavior on height { NumberAnimation { duration: Components.Theme.animationNormal; easing.type: Easing.OutCubic } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        spacing: 12
                        Text {
                            text: root.musicEnabled ? "♫" : "◌"
                            color: root.musicEnabled ? Components.Theme.shellIconAccent : Qt.rgba(1, 1, 1, 0.35)
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: 21
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                text: root.musicEnabled ? I18n.tr("settings.island.now_playing", "Now playing") : I18n.tr("settings.island.island_is_ready", "Island is ready")
                                color: "#f5f5f7"
                                font.family: Components.Theme.typography.role("caption").family
                                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                lineHeight: Components.Theme.typography.role("caption").lineHeight
                                lineHeightMode: Text.ProportionalHeight
                                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                font.weight: Components.Theme.typography.role("caption").weight
                            }
                            Text {
                                text: root.musicEnabled ? "Aster · Midnight Drive" : I18n.tr("settings.island.preview_mode", "Preview mode")
                                color: Qt.rgba(1, 1, 1, 0.60)
                                font.family: Components.Theme.typography.role("caption").family
                                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                lineHeight: Components.Theme.typography.role("caption").lineHeight
                                lineHeightMode: Text.ProportionalHeight
                                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                        Text {
                            visible: root.gamemodeNotifications
                            text: "●"
                            color: Components.Theme.accent
                            font.pixelSize: 9
                        }
                    }
                }
            }
        }

        Form.SectionHeader { text: I18n.tr("settings.island.system", "SYSTEM"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "islandSystemCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "enableIslandRow"
                label: I18n.tr("settings.island.enable_island", "Enable Island")
                sublabel: I18n.tr("settings.island.start_with_the_system_and_display_the_island", "Start with the system and display the Island.")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("settings.island.enable_island", "Enable Island")
                    checked: root.islandEnabled
                    onToggled: checked => root.islandEnabled = checked
                }
            }
            Form.SettingRow {
                objectName: "alwaysOnTopRow"
                label: I18n.tr("settings.island.always_on_top", "Always on Top")
                sublabel: I18n.tr("settings.island.keep_the_island_above_other_windows", "Keep the Island above other windows.")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("settings.island.always_on_top", "Always on Top")
                    checked: root.alwaysOnTop
                    onToggled: checked => root.alwaysOnTop = checked
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: I18n.tr("settings.island.island_look_feel", "ISLAND LOOK & FEEL"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "islandAppearanceCard"
            Layout.bottomMargin: 24
            Form.SettingRow {
                objectName: "islandStyleRow"
                label: I18n.tr("settings.island.style", "Style")
                sublabel: I18n.tr("settings.island.choose_the_overall_shape_of_the_island", "Choose the overall shape of the Island.")
                Controls.SelectButton {
                    objectName: "islandStyleSelect"
                    width: 180
                    options: root.styles
                    selectedIndex: root.style
                    label: root.styles[root.style]
                    onSelected: index => root.style = index
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: I18n.tr("settings.island.features", "FEATURES"); Layout.bottomMargin: 12 }
        Form.FormCard {
            objectName: "islandFeaturesCard"
            Form.SettingRow {
                objectName: "islandMusicRow"
                label: I18n.tr("settings.island.music", "Music")
                sublabel: I18n.tr("settings.island.display_now_playing_information", "Display now playing information.")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("settings.island.music", "Music")
                    checked: root.musicEnabled
                    onToggled: checked => root.musicEnabled = checked
                }
            }
            Form.SettingRow {
                objectName: "islandGamemodeRow"
                label: I18n.tr("settings.island.gamemode_notify", "Gamemode Notify")
                sublabel: I18n.tr("settings.island.show_alerts_when_entering_or_leaving_game_mo", "Show alerts when entering or leaving game mode.")
                Controls.ToggleSwitch {
                    accessibleName: I18n.tr("settings.island.gamemode_notify", "Gamemode Notify")
                    checked: root.gamemodeNotifications
                    onToggled: checked => root.gamemodeNotifications = checked
                }
                isLast: true
            }
        }
    }
}
