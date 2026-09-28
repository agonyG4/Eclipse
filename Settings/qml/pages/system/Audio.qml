import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import Astrea.System 1.0
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

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
    readonly property int outputCount:
        root.audio && root.audio.outputsModel ? root.audio.outputsModel.rowCount() : 0

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
        return I18n.tr("apps.settings.pages.system.audio.status.ready", "Audio is ready")
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.system.audio.section.status", "AUDIO STATUS")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 24
            margins: Components.Theme.spacingLarge

            ColumnLayout {
                width: parent.width
                spacing: Components.Theme.spacingMicro

                Text {
                    objectName: "audioServiceStatus"
                    Layout.fillWidth: true
                    text: root.statusText()
                    color: root.audio && root.audio.available
                        ? Components.Theme.textPrimary : Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeLarge
                    font.weight: Components.Theme.fontWeightMedium
                    wrapMode: Text.WordWrap
                }

                Text {
                    objectName: "audioServiceError"
                    Layout.fillWidth: true
                    visible: root.audio
                        && (root.serviceState === System.Unavailable
                            || root.serviceState === System.Degraded)
                        && root.audio.errorString !== ""
                    text: root.audio ? root.audio.errorString : ""
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.system.audio.section.volume", "VOLUME")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 24

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.audio.label.system_volume",
                               "System volume")
                sublabel: root.hasDefaultState
                    ? I18n.tr("apps.settings.pages.system.audio.sublabel.system_volume",
                              "Adjust playback volume for the current default output.")
                    : I18n.tr("apps.settings.pages.system.audio.status.waiting",
                              "Waiting for current audio state…")

                Controls.Slider {
                    id: volumeSlider
                    objectName: "audioVolumeSlider"
                    width: 280
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

                Text {
                    objectName: "audioVolumeLoading"
                    visible: !root.hasDefaultState
                    text: I18n.tr("apps.settings.pages.system.audio.status.waiting",
                                  "Waiting for current audio state…")
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                }
            }

            Form.SettingRow {
                label: I18n.tr("apps.settings.pages.system.audio.label.mute", "Mute")
                sublabel: root.hasDefaultState
                    ? I18n.tr("apps.settings.pages.system.audio.sublabel.mute",
                              "Mute the current default output.")
                    : I18n.tr("apps.settings.pages.system.audio.status.waiting",
                              "Waiting for current audio state…")
                isLast: true

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

                Text {
                    objectName: "audioMuteLoading"
                    visible: !root.hasDefaultState
                    text: I18n.tr("apps.settings.pages.system.audio.status.waiting",
                                  "Waiting for current audio state…")
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.system.audio.section.outputs", "OUTPUT DEVICES")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            margins: Components.Theme.spacingTiny

            Repeater {
                model: root.audio ? root.audio.outputsModel : null

                delegate: QQC2.Button {
                    id: outputButton
                    required property int nodeId
                    required property string displayName
                    required property string name
                    required property string description
                    required property string nick
                    required property string mediaClass
                    required property bool isDefault
                    required property bool isVirtual

                    objectName: "audioOutputRow_" + nodeId
                    Layout.fillWidth: true
                    Layout.preferredHeight: 68
                    enabled: root.audio
                        ? root.audio.available && root.audio.ready
                            && root.serviceState === System.Ready : false
                    flat: true
                    Accessible.name: displayName.trim().length > 0
                        ? displayName
                        : I18n.tr("apps.settings.pages.system.audio.label.output_fallback",
                                  "Audio output")
                    Accessible.description: isDefault
                        ? I18n.tr("apps.settings.pages.system.audio.label.default", "Default")
                        : I18n.tr("apps.settings.pages.system.audio.action.set_default",
                                  "Set as default output")

                    background: Rectangle {
                        radius: Components.Theme.controlRadius
                        color: outputButton.isDefault
                            ? Qt.rgba(Components.Theme.accent.r,
                                      Components.Theme.accent.g,
                                      Components.Theme.accent.b,
                                      Components.Theme.isLight ? 0.1 : 0.16)
                            : outputButton.hovered
                                ? Qt.rgba(Components.Theme.textPrimary.r,
                                          Components.Theme.textPrimary.g,
                                          Components.Theme.textPrimary.b, 0.045)
                                : "transparent"
                        border.width: outputButton.activeFocus ? 1 : 0
                        border.color: Components.Theme.accent
                    }

                    contentItem: RowLayout {
                        spacing: Components.Theme.spacingLarge

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Components.Theme.spacingMicro

                            Text {
                                objectName: "audioOutputName_" + outputButton.nodeId
                                Layout.fillWidth: true
                                text: outputButton.displayName.trim().length > 0
                                    ? outputButton.displayName
                                    : I18n.tr("apps.settings.pages.system.audio.label.output_fallback",
                                              "Audio output")
                                color: Components.Theme.textPrimary
                                font.family: Components.Theme.fontFamily
                                font.pixelSize: Components.Theme.fontSizeLarge
                                font.weight: Components.Theme.fontWeightMedium
                                elide: Text.ElideRight
                            }

                            Text {
                                objectName: "audioOutputDetail_" + outputButton.nodeId
                                Layout.fillWidth: true
                                visible: outputButton.isVirtual
                                    || (outputButton.nick.trim().length > 0
                                        && outputButton.nick.trim()
                                            !== outputButton.displayName.trim())
                                text: outputButton.isVirtual
                                    ? I18n.tr("apps.settings.pages.system.audio.label.virtual_output",
                                              "Virtual audio output")
                                    : outputButton.nick.trim()
                                color: Components.Theme.textSecondary
                                font.family: Components.Theme.fontFamily
                                font.pixelSize: Components.Theme.fontSizeSmall
                                elide: Text.ElideRight
                            }
                        }

                        Text {
                            objectName: "audioOutputDefault_" + outputButton.nodeId
                            visible: outputButton.isDefault
                            text: I18n.tr("apps.settings.pages.system.audio.label.default", "Default")
                            color: Components.Theme.accent
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeSmall
                            font.weight: Components.Theme.fontWeightMedium
                        }

                        Text {
                            visible: outputButton.isDefault
                            text: "✓"
                            color: Components.Theme.accent
                            font.pixelSize: Components.Theme.fontSizeLarge
                            Accessible.ignored: true
                        }
                    }

                    onClicked: {
                        if (root.audio && !isDefault)
                            root.audio.setDefaultOutput(nodeId)
                    }
                }
            }

            Text {
                objectName: "audioNoOutputs"
                Layout.fillWidth: true
                Layout.margins: Components.Theme.spacingLarge
                visible: root.audio && root.audio.available && root.outputCount === 0
                text: I18n.tr("apps.settings.pages.system.audio.status.no_outputs",
                              "No audio outputs found")
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeMedium
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                objectName: "audioOutputsUnavailable"
                Layout.fillWidth: true
                Layout.margins: Components.Theme.spacingLarge
                visible: !root.audio || !root.audio.available
                text: I18n.tr("apps.settings.pages.system.audio.status.outputs_unavailable",
                              "Audio outputs are unavailable")
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeMedium
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.system.audio.section.spatial_audio", "SPATIAL AUDIO")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 24

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

            Text {
                objectName: "spatialAudioConfigError"
                Layout.fillWidth: true
                Layout.leftMargin: Components.Theme.spacingLarge
                Layout.rightMargin: Components.Theme.spacingLarge
                Layout.bottomMargin: Components.Theme.spacingLarge
                visible: root.spatialAudio
                    && root.spatialAudio.spatialAudioConfigError !== ""
                text: root.spatialAudio
                    ? root.spatialAudio.spatialAudioConfigError : ""
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }
        }
    }
}
