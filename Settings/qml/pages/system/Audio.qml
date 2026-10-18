import QtQuick
import Qt5Compat.GraphicalEffects as Legacy
import QtQuick.Layouts
import Astrea.System 1.0
import "../../components" as Components
import "../../components/audio" as AudioComponents
import "../../components/controls" as Controls
import "../../components/form" as Form
import "../../components/typography" as Typography

FocusScope {
    id: root
    objectName: "audioPage"

    readonly property var audio:
        typeof SettingsController !== "undefined" && SettingsController
            ? SettingsController.audio : null
    readonly property var spatialAudio:
        typeof SettingsController !== "undefined" && SettingsController
            ? SettingsController.spatialAudio : null
    readonly property bool hasDefaultState: root.audio ? root.audio.defaultStateAvailable : false
    readonly property bool canMutateDefault:
        root.audio ? root.audio.available && root.audio.ready
            && root.serviceState === System.Ready && root.audio.defaultStateAvailable : false
    readonly property int serviceState:
        root.audio ? root.audio.state : System.Stopped
    readonly property int outputCount: outputRepeater.count
    readonly property int inputCount: inputRepeater.count
    readonly property bool inputReady: root.audio && root.audio.available && root.audio.ready
        && root.serviceState === System.Ready && root.audio.inputStateAvailable

    function statusText() {
        if (!root.audio || root.serviceState === System.Unavailable)
            return I18n.tr("apps.settings.pages.system.audio.status.unavailable",
                           "Audio service is unavailable")
        if (root.serviceState === System.Starting)
            return I18n.tr("apps.settings.pages.system.audio.status.connecting",
                           "Connecting to audio…")
        if (root.serviceState === System.Degraded)
            return I18n.tr("apps.settings.pages.system.audio.status.reconnecting",
                           "Reconnecting to audio…")
        if (root.serviceState === System.Stopped)
            return I18n.tr("apps.settings.pages.system.audio.status.inactive",
                           "Audio service is inactive")
        if (!root.hasDefaultState)
            return I18n.tr("apps.settings.pages.system.audio.status.waiting",
                           "Waiting for current audio state…")
        return ""
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: Components.Theme.spacingXLarge
            spacing: Components.Theme.spacingLarge

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Components.Theme.spacingMicro

                Typography.DisplayLabel {
                    objectName: "audioPageTitle"
                    text: I18n.tr("settings.audio.title", "Audio")
                    font.pixelSize: Components.Theme.fontSizeHero
                    font.weight: Components.Theme.fontWeightDemiBold
                }

                Typography.TextLabel {
                    text: I18n.tr("settings.audio.subtitle",
                                  "Choose an output device and adjust your sound.")
                    textColor: Components.Theme.textSecondary
                    font.pixelSize: Components.Theme.fontSizeNormal
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

        }

        Typography.TextLabel {
            objectName: "audioServiceStatus"
            Layout.fillWidth: true
            Layout.bottomMargin: Components.Theme.spacingSmall
            visible: text.length > 0
            text: root.statusText()
            textColor: Components.Theme.textSecondary
            font.pixelSize: Components.Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        Typography.TextLabel {
            objectName: "audioServiceError"
            Layout.fillWidth: true
            Layout.bottomMargin: Components.Theme.spacingMedium
            visible: root.audio
                && (root.serviceState === System.Unavailable
                    || root.serviceState === System.Degraded)
                && root.audio.errorString !== ""
            text: root.audio ? root.audio.errorString : ""
            textColor: Components.Theme.warningColor
            font.pixelSize: Components.Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }

        Form.SectionHeader {
            text: I18n.tr("settings.audio.section.output", "OUTPUT")
            Layout.bottomMargin: Components.Theme.spacingMedium
        }

        Form.FormCard {
            Layout.bottomMargin: Components.Theme.spacingXLarge
            margins: Components.Theme.spacingTiny

            Repeater {
                id: outputRepeater
                model: root.audio ? root.audio.outputsModel : null

                delegate: AudioComponents.DeviceRow {
                    lastInGroup: index === root.outputCount - 1
                    enabled: root.audio
                        ? root.audio.available && root.audio.ready
                            && root.serviceState === System.Ready : false
                    onActivated: selectedNodeId => {
                        if (root.audio && !isDefault)
                            root.audio.setDefaultOutput(selectedNodeId)
                    }
                }
            }

            Typography.TextLabel {
                objectName: "audioNoOutputs"
                Layout.fillWidth: true
                Layout.margins: Components.Theme.spacingLarge
                visible: root.audio && root.audio.available && root.outputCount === 0
                text: I18n.tr("apps.settings.pages.system.audio.status.no_outputs",
                              "No audio outputs found")
                textColor: Components.Theme.textSecondary
                font.pixelSize: Components.Theme.fontSizeNormal
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Typography.TextLabel {
                objectName: "audioOutputsUnavailable"
                Layout.fillWidth: true
                Layout.margins: Components.Theme.spacingLarge
                visible: !root.audio || !root.audio.available
                text: I18n.tr("apps.settings.pages.system.audio.status.outputs_unavailable",
                              "Audio outputs are unavailable")
                textColor: Components.Theme.textSecondary
                font.pixelSize: Components.Theme.fontSizeNormal
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }

        Form.SectionHeader {
            text: I18n.tr("settings.audio.section.input", "INPUT")
            Layout.bottomMargin: Components.Theme.spacingMedium
        }

        Form.FormCard {
            Layout.bottomMargin: Components.Theme.spacingXLarge
            margins: Components.Theme.spacingTiny

            Repeater {
                id: inputRepeater
                model: root.audio ? root.audio.inputsModel : null
                delegate: AudioComponents.DeviceRow {
                    inputDevice: true
                    lastInGroup: index === root.inputCount - 1
                    enabled: root.audio && root.audio.available
                        && root.audio.ready && root.serviceState === System.Ready
                    onActivated: selectedNodeId => {
                        if (root.audio && !isDefault)
                            root.audio.setDefaultInput(selectedNodeId)
                    }
                }
            }

            Typography.TextLabel {
                objectName: "audioNoInputs"
                Layout.fillWidth: true
                Layout.margins: Components.Theme.spacingLarge
                visible: root.audio && root.audio.available && root.inputCount === 0
                text: I18n.tr("settings.audio.no_inputs", "No microphones found")
                textColor: Components.Theme.textSecondary
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Form.SectionHeader {
            text: I18n.tr("settings.audio.section.input_level", "INPUT LEVEL")
            Layout.bottomMargin: Components.Theme.spacingMedium
        }
        Form.FormCard {
            Layout.bottomMargin: Components.Theme.spacingXLarge
            Form.SettingRow {
                label: I18n.tr("settings.audio.input_volume", "Microphone volume")
                sublabel: I18n.tr("settings.audio.input_volume_description", "Adjust the selected microphone's input gain.")
                RowLayout {
                    spacing: Components.Theme.spacingMedium
                    Controls.Slider {
                        objectName: "audioInputVolumeSlider"
                        Layout.preferredWidth: 200
                        from: 0
                        to: 150
                        stepSize: 1
                        detentEnabled: true
                        detentValue: 100
                        modelValueEnabled: true
                        modelValue: root.audio ? root.audio.inputVolume : 0
                        showEndpointGlyphs: false
                        showValueTooltip: false
                        enabled: root.inputReady
                        onValueEdited: value => {
                            if (root.inputReady) root.audio.setInputVolume(value)
                        }
                    }
                    Typography.TextLabel {
                        objectName: "audioInputVolumePercent"
                        Layout.preferredWidth: 44
                        text: root.inputReady ? Math.round(root.audio.inputVolume) + "%" : "—"
                        textColor: Components.Theme.textSecondary
                    }
                }
            }
            Form.SettingRow {
                label: I18n.tr("settings.audio.input_mute", "Mute microphone")
                isLast: true
                Controls.ToggleSwitch {
                    objectName: "audioInputMuteToggle"
                    checked: root.audio ? root.audio.inputMuted : false
                    enabled: root.inputReady
                    accessibleName: "Mute microphone"
                    onToggled: checked => {
                        if (root.inputReady) root.audio.setInputMuted(checked)
                    }
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.system.audio.section.volume", "VOLUME")
            Layout.bottomMargin: Components.Theme.spacingMedium
        }

        Form.FormCard {
            Layout.bottomMargin: Components.Theme.spacingXLarge

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.audio.label.system_volume",
                               "System volume")
                sublabel: I18n.tr("apps.settings.pages.system.audio.sublabel.system_volume",
                                  "Adjust playback volume for the current default output.")

                RowLayout {
                    spacing: Components.Theme.spacingMedium

                    Item {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 18
                        Layout.alignment: Qt.AlignVCenter
                        visible: root.hasDefaultState

                        Image {
                            id: volumeMinAppleIcon
                            objectName: "audioVolumeMinAppleIcon"
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            readonly property bool appleSelected:
                                SettingsController.navigationIconTheme === "apple"
                            source: appleSelected ? SettingsController.iconUrlForWeight(
                                "volume-min", "apple", SettingsController.navigationIconWeight) : ""
                            sourceSize: Qt.size(width, height)
                            fillMode: Image.PreserveAspectFit
                            visible: appleSelected && status === Image.Ready
                            layer.enabled: visible
                            layer.effect: Legacy.ColorOverlay {
                                color: Components.Theme.textSecondary
                            }
                            Accessible.ignored: true
                        }

                        Text {
                            objectName: "audioVolumeLowIcon"
                            anchors.centerIn: parent
                            visible: !volumeMinAppleIcon.visible
                            text: "\uf027" // volume-low, Lucide fallback
                            font.family: "JetBrainsMono Nerd Font"
                            font.pixelSize: 16
                            color: Components.Theme.textSecondary
                            Accessible.ignored: true
                        }
                    }

                    Controls.Slider {
                        id: volumeSlider
                        objectName: "audioVolumeSlider"
                        Layout.preferredWidth: 200
                        Layout.minimumWidth: 120
                        showEndpointGlyphs: false
                        showValueTooltip: false
                        from: 0
                        to: 150
                        stepSize: 1
                        detentEnabled: true
                        detentValue: 100
                        modelValueEnabled: true
                        modelValue: root.audio ? root.audio.volume : 0
                        valueText: root.hasDefaultState
                            ? Math.round(root.audio.volume) + "%" : ""
                        enabled: root.canMutateDefault
                        visible: root.hasDefaultState
                        Accessible.name: I18n.tr("apps.settings.pages.system.audio.label.system_volume",
                                                 "System volume")
                        onValueEdited: value => {
                            if (root.canMutateDefault)
                                root.audio.setVolume(value)
                        }
                    }

                    Item {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 18
                        Layout.alignment: Qt.AlignVCenter
                        visible: root.hasDefaultState

                        Image {
                            id: volumeMaxAppleIcon
                            objectName: "audioVolumeMaxAppleIcon"
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            readonly property bool appleSelected:
                                SettingsController.navigationIconTheme === "apple"
                            source: appleSelected ? SettingsController.iconUrlForWeight(
                                "volume-max", "apple", SettingsController.navigationIconWeight) : ""
                            sourceSize: Qt.size(width, height)
                            fillMode: Image.PreserveAspectFit
                            visible: appleSelected && status === Image.Ready
                            layer.enabled: visible
                            layer.effect: Legacy.ColorOverlay {
                                color: Components.Theme.textSecondary
                            }
                            Accessible.ignored: true
                        }

                        Text {
                            objectName: "audioVolumeHighIcon"
                            anchors.centerIn: parent
                            visible: !volumeMaxAppleIcon.visible
                            text: "\uf028" // volume-high, Lucide fallback
                            font.family: "JetBrainsMono Nerd Font"
                            font.pixelSize: 16
                            color: Components.Theme.textSecondary
                            Accessible.ignored: true
                        }
                    }

                    Typography.TextLabel {
                        objectName: "audioVolumePercent"
                        visible: root.hasDefaultState
                        text: root.audio ? Math.round(root.audio.volume) + "%" : ""
                        textColor: Components.Theme.textPrimary
                        font.pixelSize: Components.Theme.fontSizeNormal
                        font.weight: Components.Theme.fontWeightMedium
                        Layout.preferredWidth: 43
                        horizontalAlignment: Text.AlignRight
                    }

                    Typography.TextLabel {
                        objectName: "audioVolumeLoading"
                        visible: !root.hasDefaultState
                        text: I18n.tr("apps.settings.pages.system.audio.status.waiting",
                                      "Waiting for current audio state…")
                        textColor: Components.Theme.textSecondary
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.audio.label.mute", "Mute")
                sublabel: I18n.tr("apps.settings.pages.system.audio.sublabel.mute",
                                  "Mute the current default output.")
                isLast: true

                RowLayout {
                    Controls.ToggleSwitch {
                        id: muteToggle
                        objectName: "audioMuteToggle"
                        checked: root.audio ? root.audio.muted : false
                        enabled: root.canMutateDefault
                        visible: root.hasDefaultState
                        accessibleName: I18n.tr("apps.settings.pages.system.audio.label.mute", "Mute")
                        onToggled: targetChecked => {
                            if (root.canMutateDefault)
                                root.audio.setMuted(targetChecked)
                        }
                    }

                    Typography.TextLabel {
                        objectName: "audioMuteLoading"
                        visible: !root.hasDefaultState
                        text: I18n.tr("apps.settings.pages.system.audio.status.waiting",
                                      "Waiting for current audio state…")
                        textColor: Components.Theme.textSecondary
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.system.audio.section.spatial_audio", "SPATIAL AUDIO")
            Layout.bottomMargin: Components.Theme.spacingMedium
        }

        Form.FormCard {
            Layout.bottomMargin: Components.Theme.spacingXLarge

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.audio.label.spatial_audio",
                               "Spatial Audio")
                sublabel: root.spatialAudio && root.spatialAudio.spatialAudioEnabled
                    ? I18n.tr("apps.settings.pages.system.audio.sublabel.spatial_audio.enabled",
                              "Creates the Astrea 7.1 spatial output while the Astrea audio runtime is active.")
                    : I18n.tr("apps.settings.pages.system.audio.sublabel.spatial_audio.disabled",
                              "Enable Astrea binaural spatial processing.")
                isLast: true

                Controls.ToggleSwitch {
                    objectName: "spatialAudioToggle"
                    checked: root.spatialAudio
                        ? root.spatialAudio.spatialAudioEnabled : false
                    enabled: root.spatialAudio !== null
                    accessibleName: I18n.tr("apps.settings.pages.system.audio.label.spatial_audio",
                                            "Spatial Audio")
                    onToggled: targetChecked => {
                        if (root.spatialAudio)
                            root.spatialAudio.setSpatialAudioEnabled(targetChecked)
                    }
                }
            }

            Typography.TextLabel {
                objectName: "spatialAudioConfigError"
                Layout.fillWidth: true
                Layout.leftMargin: Components.Theme.spacingLarge
                Layout.rightMargin: Components.Theme.spacingLarge
                Layout.bottomMargin: Components.Theme.spacingLarge
                visible: root.spatialAudio
                    && root.spatialAudio.spatialAudioConfigError !== ""
                text: root.spatialAudio
                    ? root.spatialAudio.spatialAudioConfigError : ""
                textColor: Components.Theme.warningColor
                font.pixelSize: Components.Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }
        }
    }
}
