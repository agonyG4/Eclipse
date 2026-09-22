import QtQuick
import QtQuick.Layouts
import "../../components" as Components
import "../../components/form" as Form

Item {
    id: root
    objectName: "visualEffectsPage"
    readonly property var wallpaperController: SettingsController.wallpaper

    Component.onCompleted: {
        if (root.wallpaperController && !root.wallpaperController.busy)
            root.wallpaperController.refresh()
    }

    component MaterialOption: FocusScope {
        id: option
        property string optionId: ""
        property string label: ""
        property int styleValue: 1
        readonly property bool selected: Components.Theme.shellStyle === styleValue

        activeFocusOnTab: true
        implicitHeight: 168
        Layout.fillWidth: true
        Layout.minimumWidth: 0

        Accessible.role: Accessible.Button
        Accessible.name: option.label

        function activate() {
            option.forceActiveFocus()
            Components.Theme.setShellStyle(option.styleValue)
            Components.Theme.save()
        }

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                option.activate()
                event.accepted = true
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: Components.Theme.cardRadius
            color: Components.Theme.cardBg
            border.width: option.selected || option.activeFocus ? 2 : 1
            border.color: option.selected || option.activeFocus
                ? Components.Theme.accent : Components.Theme.cardBorder
            scale: optionMouse.pressed ? 0.985 : 1

            Behavior on border.color {
                ColorAnimation { duration: Components.Theme.animationQuick; easing.type: Easing.OutCubic }
            }
            Behavior on scale {
                NumberAnimation { duration: Components.Theme.animationQuick; easing.type: Easing.OutCubic }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                MaterialPreview {
                    objectName: "materialPreview-" + option.optionId
                    Layout.fillWidth: true
                    Layout.preferredHeight: 112
                    wallpaperSource: root.wallpaperController
                        ? root.wallpaperController.effectivePreviewUrl : ""
                    wallpaperFit: root.wallpaperController
                        ? root.wallpaperController.effectiveFit : "cover"
                    themeVariant: Components.Theme.isLight ? "light" : "dark"
                    materialId: option.optionId
                }

                Text {
                    Layout.fillWidth: true
                    text: option.label
                    color: Components.Theme.textPrimary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    font.weight: option.selected
                        ? Components.Theme.fontWeightDemiBold : Components.Theme.fontWeightMedium
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                visible: option.selected
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 8
                width: 18
                height: 18
                radius: 9
                color: Components.Theme.accent

                Text {
                    anchors.centerIn: parent
                    text: "\uf00c"
                    color: Components.Theme.accentForeground
                    font.family: Components.Theme.monoFontFamily
                    font.pixelSize: 10
                    font.weight: Components.Theme.fontWeightBold
                }
            }

            MouseArea {
                id: optionMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: option.activate()
            }
        }
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 760

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.visual_effects.text.interface_style", "INTERFACE STYLE")
            Layout.bottomMargin: 10
        }

        Form.FormCard {
            Layout.fillWidth: true
            Layout.bottomMargin: 20
            margins: 8

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 10
                rowSpacing: 10

                MaterialOption {
                    objectName: "materialOption-default"
                    optionId: "default"
                    styleValue: 1
                    label: I18n.tr("apps.settings.pages.visual_effects.option.default", "Default")
                }

                MaterialOption {
                    objectName: "materialOption-transparent"
                    optionId: "transparent"
                    styleValue: 0
                    label: I18n.tr("apps.settings.pages.visual_effects.option.transparent", "Transparent")
                }

                MaterialOption {
                    objectName: "materialOption-frosted"
                    optionId: "frosted"
                    styleValue: 2
                    label: I18n.tr("apps.settings.pages.visual_effects.option.frosted", "Frosted")
                }
            }
        }
    }
}
