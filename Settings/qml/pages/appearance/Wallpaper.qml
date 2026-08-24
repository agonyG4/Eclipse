import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "wallpaperPage"

    readonly property var controller: SettingsController.wallpaper
    readonly property var fitOptions: [
        I18n.tr("apps.settings.pages.appearance.wallpaper.fit.cover", "Cover"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.fit.contain", "Contain"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.fit.stretch", "Stretch"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.fit.center", "Center"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.fit.tile", "Tile")
    ]
    readonly property var fitValues: ["cover", "contain", "stretch", "center", "tile"]
    property int selectedFit: 0

    function syncFitFromController() {
        var authoritativeFit = root.controller.configuredFit !== ""
                ? root.controller.configuredFit
                : root.controller.effectiveFit
        var index = root.fitValues.indexOf(authoritativeFit)
        if (index >= 0)
            root.selectedFit = index
    }

    Component.onCompleted: {
        root.syncFitFromController()
        root.controller.refreshLibrary()
    }

    Connections {
        target: root.controller
        function onSnapshotChanged() {
            root.syncFitFromController()
        }
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.wallpaper.title", "WALLPAPER")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 24

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 260
                radius: Components.Theme.cardRadius
                color: Components.Theme.cardBg
                border.width: 1
                border.color: Components.Theme.cardBorder
                clip: true

                Image {
                    id: preview
                    anchors.fill: parent
                    anchors.margins: 1
                    source: root.controller.effectiveSource
                    asynchronous: true
                    cache: false
                    fillMode: root.controller.effectiveFit === "contain"
                              ? Image.PreserveAspectFit
                              : root.controller.effectiveFit === "stretch"
                              ? Image.Stretch
                              : root.controller.effectiveFit === "center"
                              ? Image.Pad
                              : root.controller.effectiveFit === "tile"
                              ? Image.Tile
                              : Image.PreserveAspectCrop
                    visible: status === Image.Ready
                }

                Text {
                    anchors.centerIn: parent
                    text: root.controller.busy
                          ? I18n.tr("apps.settings.pages.appearance.wallpaper.pending", "Applying wallpaper…")
                          : root.controller.stateName === "fallback"
                          ? I18n.tr("apps.settings.pages.appearance.wallpaper.fallback", "Using fallback wallpaper")
                          : I18n.tr("apps.settings.pages.appearance.wallpaper.loading", "Loading wallpaper…")
                    color: Components.Theme.textSecondary
                    visible: preview.status !== Image.Ready
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.wallpaper.library", "Wallpaper library")
                sublabel: I18n.tr("apps.settings.pages.appearance.wallpaper.library_help", "Choose a wallpaper owned by Paper.")

                Column {
                    width: 300
                    spacing: Components.Theme.spacingTiny

                    Repeater {
                        model: root.controller.wallpapers

                        delegate: Controls.Button {
                            width: 300
                            label: modelData.displayName !== ""
                                   ? modelData.displayName
                                   : modelData.logicalId
                            flat: modelData.logicalId !== root.controller.configuredId
                            enabled: !root.controller.busy
                            onClicked: root.controller.selectWallpaper(
                                           modelData.logicalId,
                                           root.fitValues[root.selectedFit])
                        }
                    }
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.wallpaper.source", "Configured source")
                sublabel: root.controller.stateName === "fallback"
                          ? I18n.tr("apps.settings.pages.appearance.wallpaper.source_missing", "The configured image is unavailable; it has not been removed.")
                          : I18n.tr("apps.settings.pages.appearance.wallpaper.source_help", "Choose a local image file or enter a path.")

                Row {
                    spacing: Components.Theme.spacingSmall

                    TextField {
                        id: sourceInput
                        width: 220
                        text: root.controller.configuredSource
                        placeholderText: I18n.tr("apps.settings.pages.appearance.wallpaper.path_placeholder", "Image path")
                        enabled: !root.controller.busy
                        onAccepted: root.controller.setSource(text, root.fitValues[root.selectedFit])
                    }

                    Controls.Button {
                        label: I18n.tr("apps.settings.pages.appearance.wallpaper.import", "Import")
                        enabled: !root.controller.busy
                        onClicked: root.controller.importWallpaper(
                                       sourceInput.text,
                                       root.fitValues[root.selectedFit])
                    }
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.wallpaper.fit", "Image fit")
                sublabel: I18n.tr("apps.settings.pages.appearance.wallpaper.fit_help", "Control how the image fills each output.")

                Form.SelectButton {
                    width: 180
                    label: root.fitOptions[root.selectedFit]
                    options: root.fitOptions
                    selectedIndex: root.selectedFit
                    enabled: !root.controller.busy
                    onSelected: index => root.selectedFit = index
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.appearance.wallpaper.actions", "Wallpaper actions")
                sublabel: root.controller.errorMessage !== ""
                          ? root.controller.errorMessage
                          : I18n.tr("apps.settings.pages.appearance.wallpaper.status", "Changes are validated before they become active.")
                isLast: true

                Row {
                    spacing: Components.Theme.spacingSmall

                    Controls.Button {
                        label: I18n.tr("apps.settings.pages.appearance.wallpaper.apply", "Apply")
                        primary: true
                        enabled: !root.controller.busy
                        onClicked: root.controller.setSource(sourceInput.text, root.fitValues[root.selectedFit])
                    }
                    Controls.Button {
                        label: I18n.tr("apps.settings.pages.appearance.wallpaper.reset", "Reset")
                        flat: true
                        enabled: !root.controller.busy
                        onClicked: root.controller.reset()
                    }
                }
            }
        }
    }

}
