import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../" as Components
import "../controls" as Controls

// Compact, keyboard-accessible device selection shared by audio device groups.
Item {
    id: root

    required property int index
    required property int nodeId
    required property string displayName
    required property string name
    required property string description
    required property string nick
    required property string mediaClass
    required property bool isDefault
    required property bool isVirtual

    property bool inputDevice: false
    property bool selected: false
    property bool lastInGroup: false
    signal activated(int nodeId)

    readonly property string deviceTitle: displayName.trim().length > 0
        ? displayName.trim()
        : I18n.tr("apps.settings.pages.system.audio.label.output_fallback", "Audio output")
    readonly property string deviceDetail: {
        if (inputDevice) return "\uf130" // microphone
        if (isVirtual)
            return I18n.tr("apps.settings.pages.system.audio.label.virtual_output",
                           "Virtual audio output")
        const nickname = nick.trim()
        if (nickname.length > 0 && nickname !== deviceTitle)
            return nickname
        const detail = description.trim()
        return detail !== deviceTitle ? detail : ""
    }
    readonly property string kind: (name + " " + deviceTitle + " "
                                    + mediaClass + " " + nick).toLowerCase()
    readonly property string deviceGlyph: {
        if (inputDevice) return "\uf130" // microphone
        if (isVirtual)
            return "\uf1de" // sliders
        // Some USB headset adapters identify as sound cards, not headphones.
        if (kind.includes("headset") || kind.includes("headphone")
                || kind.includes("earbud") || kind.includes("airpods")
                || kind.includes("hs60") || kind.includes("hs70")
                || kind.includes("hs80"))
            return "\uee59" // Font Awesome headset with microphone (Nerd Font)
        if (kind.includes("hdmi") || kind.includes("display")
                || kind.includes("monitor"))
            return "\uf108" // desktop
        return "\uf028" // volume-up
    }

    readonly property string deviceIconKey: {
        if (inputDevice) return "microphone"
        if (isVirtual) return "virtual-device"
        if (kind.includes("headset") || kind.includes("headphone")
                || kind.includes("earbud") || kind.includes("airpods")
                || kind.includes("hs60") || kind.includes("hs70")
                || kind.includes("hs80"))
            return "headphones"
        if (kind.includes("hdmi") || kind.includes("display")
                || kind.includes("monitor"))
            return "display"
        return "audio"
    }
    readonly property url deviceIconSource: typeof SettingsController !== "undefined"
        && SettingsController !== null
        ? SettingsController.iconUrlForWeight(deviceIconKey,
            SettingsController.navigationIconTheme, SettingsController.navigationIconWeight)
        : ""

    objectName: (inputDevice ? "audioInputRow_" : "audioOutputRow_") + nodeId
    Layout.fillWidth: true
    Layout.preferredHeight: 56
    implicitHeight: 56

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Components.Theme.spacingMedium
        anchors.rightMargin: Components.Theme.spacingMedium
        spacing: Components.Theme.spacingMedium

        QQC2.Button {
            id: deviceSelector
            objectName: (root.inputDevice ? "audioInputSelector_" : "audioOutputSelector_") + root.nodeId
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 34
            implicitHeight: 34
            padding: 0
            spacing: 0
            enabled: root.enabled
            hoverEnabled: true
            focusPolicy: Qt.StrongFocus
            Accessible.name: root.deviceTitle
            Accessible.description: root.inputDevice
                ? I18n.tr("settings.audio.select_input", "Select input device")
                : I18n.tr("settings.audio.select_output", "Select output device")

            background: Rectangle {
                radius: width / 2
                color: root.selected
                    ? Components.Theme.accent
                    : Qt.rgba(Components.Theme.textPrimary.r,
                              Components.Theme.textPrimary.g,
                              Components.Theme.textPrimary.b,
                              deviceSelector.hovered
                                  ? (Components.Theme.isLight ? 0.13 : 0.22)
                                  : (Components.Theme.isLight ? 0.07 : 0.13))
                border.width: deviceSelector.activeFocus ? 2 : 0
                border.color: Components.Theme.accent
                Behavior on color {
                    ColorAnimation { duration: Components.Theme.animationFast }
                }
            }

            contentItem: Item {
                Controls.ThemedIcon {
                    id: themedDeviceIcon
                    objectName: (root.inputDevice ? "audioInputArtwork_" : "audioOutputArtwork_") + root.nodeId
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    iconSource: root.deviceIconSource
                    tint: root.selected
                        ? Components.Theme.accentForeground : Components.Theme.textSecondary
                }

                Text {
                    objectName: (root.inputDevice ? "audioInputFallback_" : "audioOutputFallback_") + root.nodeId
                    anchors.centerIn: parent
                    visible: !themedDeviceIcon.ready
                    anchors.verticalCenterOffset: -1
                    text: root.deviceGlyph
                    font.family: "JetBrainsMono Nerd Font"
                    font.pixelSize: 17
                    color: root.selected
                        ? Components.Theme.accentForeground
                        : Components.Theme.textSecondary
                    Accessible.ignored: true
                }
            }

            onClicked: root.activated(root.nodeId)
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 2

            Text {
                objectName: "audioOutputName_" + root.nodeId
                Layout.fillWidth: true
                text: root.deviceTitle
                textFormat: Text.PlainText
                color: Components.Theme.textPrimary
                font.family: Components.Theme.typography.role("body").family
                font.pixelSize: Components.Theme.typography.role("body").pixelSize
                lineHeight: Components.Theme.typography.role("body").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("body").letterSpacing
                font.weight: Components.Theme.typography.role("body").weight
                elide: Text.ElideRight
            }

            Text {
                objectName: "audioOutputDetail_" + root.nodeId
                Layout.fillWidth: true
                visible: root.deviceDetail.length > 0
                text: root.deviceDetail
                textFormat: Text.PlainText
                color: Components.Theme.textSecondary
                font.family: Components.Theme.typography.role("caption").family
                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                lineHeight: Components.Theme.typography.role("caption").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                elide: Text.ElideRight
            }
        }


    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: Components.Theme.spacingMedium + 34
                            + Components.Theme.spacingMedium
        anchors.right: parent.right
        height: 1
        visible: !root.lastInGroup
        color: Qt.rgba(Components.Theme.textSecondary.r,
                       Components.Theme.textSecondary.g,
                       Components.Theme.textSecondary.b, 0.13)
    }
}
