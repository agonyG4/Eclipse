import QtQuick
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

FocusScope {
    id: root
    objectName: "displayPage"

    readonly property var controller:
        typeof SettingsController !== "undefined" && SettingsController
            ? SettingsController.display : null
    readonly property var outputs: root.controller ? root.controller.outputs : []
    readonly property var wallpaper: typeof SettingsController !== "undefined" && SettingsController
        ? SettingsController.wallpaper : null
    property string selectedOutputId: ""
    readonly property var selectedOutput: {
        for (let index = 0; index < root.outputs.length; ++index) {
            if (root.outputs[index].id === root.selectedOutputId)
                return root.outputs[index]
        }
        return null
    }
    readonly property int selectedIndex: {
        for (let index = 0; index < root.outputs.length; ++index) {
            if (root.outputs[index].id === root.selectedOutputId)
                return index
        }
        return -1
    }

    function nonEmpty(value) {
        return value !== undefined && value !== null && String(value).trim().length > 0
    }

    function titleFor(output) {
        if (!output)
            return "Display"
        if (root.nonEmpty(output.model))
            return String(output.model).trim()
        if (root.nonEmpty(output.make))
            return String(output.make).trim()
        return root.nonEmpty(output.name) ? String(output.name) : "Display"
    }

    function kindFor(output) {
        const name = output && root.nonEmpty(output.name) ? String(output.name) : ""
        return name.startsWith("eDP") || name.startsWith("LVDS") ? "Built-in display" : "External display"
    }

    function subtitleFor(output) {
        if (!output)
            return ""
        const pieces = [root.kindFor(output)]
        if (root.nonEmpty(output.make) && String(output.make).trim() !== root.titleFor(output))
            pieces.push(String(output.make).trim())
        if (root.nonEmpty(output.name))
            pieces.push(String(output.name))
        return pieces.join(" · ")
    }

    function outputWidth(output) {
        if (!output || !output.hasCurrentMode)
            return 240
        const width = Number(output.currentWidth) > 0 ? Number(output.currentWidth) : 240
        const height = Number(output.currentHeight) > 0 ? Number(output.currentHeight) : 135
        const transform = output ? String(output.transform) : "normal"
        return transform === "90" || transform === "270" ? height : width
    }

    function outputHeight(output) {
        if (!output || !output.hasCurrentMode)
            return 135
        const width = Number(output.currentWidth) > 0 ? Number(output.currentWidth) : 240
        const height = Number(output.currentHeight) > 0 ? Number(output.currentHeight) : 135
        const transform = output ? String(output.transform) : "normal"
        return transform === "90" || transform === "270" ? width : height
    }

    function worldMinX() {
        let value = 0
        for (let i = 0; i < root.outputs.length; ++i)
            value = Math.min(value, Number(root.outputs[i].x) || 0)
        return value
    }

    function worldMinY() {
        let value = 0
        for (let i = 0; i < root.outputs.length; ++i)
            value = Math.min(value, Number(root.outputs[i].y) || 0)
        return value
    }

    function worldWidth() {
        let value = 1
        for (let i = 0; i < root.outputs.length; ++i) {
            const output = root.outputs[i]
            value = Math.max(value, (Number(output.x) || 0) - root.worldMinX() + root.outputWidth(output))
        }
        return value
    }

    function worldHeight() {
        let value = 1
        for (let i = 0; i < root.outputs.length; ++i) {
            const output = root.outputs[i]
            value = Math.max(value, (Number(output.y) || 0) - root.worldMinY() + root.outputHeight(output))
        }
        return value
    }

    function reconcileSelection() {
        for (let i = 0; i < root.outputs.length; ++i) {
            if (root.outputs[i].id === root.selectedOutputId)
                return
        }
        for (let i = 0; i < root.outputs.length; ++i) {
            if (root.outputs[i].focused) {
                root.selectedOutputId = String(root.outputs[i].id)
                return
            }
        }
        root.selectedOutputId = root.outputs.length > 0 ? String(root.outputs[0].id) : ""
    }

    function formatRate(millihz) {
        const hz = Number(millihz) / 1000
        if (!Number.isFinite(hz) || hz <= 0)
            return "—"
        return String(Number(hz.toFixed(3)))
    }

    function formatScale(scaleMilli) {
        const percent = Number(scaleMilli) / 10
        return Number.isInteger(percent) ? String(percent) + "%" : String(Number(percent.toFixed(1))) + "%"
    }

    function orientationText(transform) {
        switch (String(transform)) {
        case "normal": return "Landscape"
        case "90": return "Portrait"
        case "180": return "Landscape flipped"
        case "270": return "Portrait flipped"
        default: return "Unknown orientation (" + String(transform) + ")"
        }
    }

    function vrrText(state) {
        switch (String(state)) {
        case "available":
        case "configured": return "Supported"
        case "active": return "Active"
        case "unavailable": return "Not supported"
        case "degraded": return "Unavailable / degraded"
        default: return "Unavailable"
        }
    }

    function resolutionGroups(output) {
        if (!output || !output.modes)
            return []
        const bySize = ({})
        for (let i = 0; i < output.modes.length; ++i) {
            const mode = output.modes[i]
            const key = String(mode.width) + "x" + String(mode.height)
            if (!bySize[key])
                bySize[key] = { width: Number(mode.width), height: Number(mode.height), preferred: false,
                                current: Number(output.currentWidth) === Number(mode.width)
                                    && Number(output.currentHeight) === Number(mode.height), rates: [] }
            const group = bySize[key]
            group.preferred = group.preferred || Boolean(mode.preferred)
            let found = false
            for (let rateIndex = 0; rateIndex < group.rates.length; ++rateIndex) {
                if (group.rates[rateIndex].milliHz === Number(mode.refreshMilliHz)
                    && group.rates[rateIndex].interlaced === Boolean(mode.interlaced)) {
                    found = true
                    break
                }
            }
            if (!found)
                group.rates.push({ milliHz: Number(mode.refreshMilliHz), interlaced: Boolean(mode.interlaced) })
        }
        const groups = Object.keys(bySize).map(key => bySize[key])
        groups.sort((left, right) => right.width * right.height - left.width * left.height
                     || right.width - left.width || right.height - left.height)
        for (let i = 0; i < groups.length; ++i)
            groups[i].rates.sort((left, right) => left.milliHz - right.milliHz || Number(left.interlaced) - Number(right.interlaced))
        return groups
    }

    Component.onCompleted: {
        root.reconcileSelection()
        if (root.controller)
            root.controller.refresh()
    }

    Connections {
        target: root.controller
        function onSnapshotChanged() { root.reconcileSelection() }
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 960

        Form.SectionHeader {
            text: "DISPLAY LAYOUT"
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "displayLayoutCard"
            Layout.bottomMargin: 24
            margins: Components.Theme.spacingLarge

            ColumnLayout {
                width: parent.width
                spacing: Components.Theme.spacingLarge

                Item {
                    id: previewCanvas
                    objectName: "displayOutputPreview"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 360

                    readonly property real canvasPadding: 30
                    readonly property real scaleFactor: root.outputs.length > 0
                        ? Math.max(0, Math.min((width - canvasPadding * 2) / root.worldWidth(),
                                               (height - canvasPadding * 2) / root.worldHeight())) : 0
                    readonly property real renderedWidth: root.worldWidth() * scaleFactor
                    readonly property real renderedHeight: root.worldHeight() * scaleFactor
                    readonly property real offsetX: (width - renderedWidth) / 2
                    readonly property real offsetY: (height - renderedHeight) / 2

                    Rectangle {
                        anchors.fill: parent
                        radius: Components.Theme.controlRadius
                        color: Components.Theme.cardBg
                        border.width: 1
                        border.color: Components.Theme.cardBorder
                    }

                    Repeater {
                        model: root.outputs

                        delegate: Item {
                            id: monitorItem
                            required property int index
                            required property var modelData
                            readonly property bool internalDisplay: root.kindFor(modelData) === "Built-in display"
                            readonly property bool hasCurrentMode: Boolean(modelData.hasCurrentMode)
                            readonly property real surfaceWidth: root.outputWidth(modelData) * previewCanvas.scaleFactor
                            readonly property real surfaceHeight: root.outputHeight(modelData) * previewCanvas.scaleFactor
                            x: previewCanvas.offsetX
                                + ((Number(modelData.x) || 0) - root.worldMinX()) * previewCanvas.scaleFactor
                            y: previewCanvas.offsetY
                                + ((Number(modelData.y) || 0) - root.worldMinY()) * previewCanvas.scaleFactor
                            width: surfaceWidth
                            height: surfaceHeight + (internalDisplay ? 12 : 10)
                            Behavior on x {
                                NumberAnimation { duration: Components.Theme.animationNormal; easing.type: Easing.OutQuart }
                            }
                            Behavior on y {
                                NumberAnimation { duration: Components.Theme.animationNormal; easing.type: Easing.OutQuart }
                            }

                            Rectangle {
                                id: monitorFrame
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.top
                                width: parent.width
                                height: monitorItem.surfaceHeight
                                radius: Math.min(Components.Theme.cardRadius, 14)
                                color: Components.Theme.cardBg
                                border.width: modelData.id === root.selectedOutputId ? 2 : 1
                                border.color: modelData.id === root.selectedOutputId
                                    ? Components.Theme.accent : Components.Theme.cardBorder
                                clip: true
                                Behavior on border.color {
                                    ColorAnimation { duration: Components.Theme.animationNormal; easing.type: Easing.OutQuart }
                                }
                                Behavior on border.width {
                                    NumberAnimation { duration: Components.Theme.animationFast; easing.type: Easing.OutCubic }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    radius: Math.max(2, monitorFrame.radius - 5)
                                    color: Components.Theme.textPrimary

                                    Rectangle {
                                        id: wallpaperFallback
                                        anchors.fill: parent
                                        visible: !modelData.enabled || !monitorItem.hasCurrentMode
                                            || !root.wallpaper || !root.wallpaper.effectivePreviewUrl
                                            || String(root.wallpaper.effectivePreviewUrl) === ""
                                            || wallpaperImage.status === Image.Error
                                        radius: parent.radius
                                        gradient: Gradient {
                                            GradientStop { position: 0; color: Components.Theme.cardBg }
                                            GradientStop { position: 1; color: Qt.darker(Components.Theme.accent, 1.25) }
                                        }
                                    }

                                    Image {
                                        id: wallpaperImage
                                        objectName: "displayWallpaperPreview"
                                        anchors.fill: parent
                                        visible: modelData.enabled && monitorItem.hasCurrentMode
                                            && root.wallpaper && root.wallpaper.effectivePreviewUrl
                                            && String(root.wallpaper.effectivePreviewUrl) !== ""
                                        source: visible ? root.wallpaper.effectivePreviewUrl : ""
                                        fillMode: {
                                            if (!root.wallpaper || root.wallpaper.effectiveFit === "cover")
                                                return Image.PreserveAspectCrop
                                            if (root.wallpaper.effectiveFit === "contain")
                                                return Image.PreserveAspectFit
                                            if (root.wallpaper.effectiveFit === "center")
                                                return Image.Pad
                                            if (root.wallpaper.effectiveFit === "tile")
                                                return Image.Tile
                                            return Image.Stretch
                                        }
                                        asynchronous: true
                                        cache: false
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        visible: !modelData.enabled || !monitorItem.hasCurrentMode
                                        text: modelData.enabled ? "No active mode" : "Disabled"
                                        textFormat: Text.PlainText
                                        color: Components.Theme.textSecondary
                                        font.family: Components.Theme.fontFamily
                                        font.pixelSize: Components.Theme.fontSizeSmall
                                    }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: 34
                                        color: Qt.rgba(Components.Theme.cardBg.r,
                                                       Components.Theme.cardBg.g,
                                                       Components.Theme.cardBg.b, 0.86)
                                        Text {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.margins: Components.Theme.spacingSmall
                                            text: (root.nonEmpty(modelData.name) ? String(modelData.name) : "Display")
                                                + (modelData.enabled && monitorItem.hasCurrentMode
                                                   ? "" : modelData.enabled ? " · No active mode" : " · Disabled")
                                            textFormat: Text.PlainText
                                            color: Components.Theme.textPrimary
                                            font.family: Components.Theme.fontFamily
                                            font.pixelSize: Components.Theme.fontSizeSmall
                                            font.weight: Components.Theme.fontWeightMedium
                                            elide: Text.ElideRight
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.selectedOutputId = String(modelData.id)
                                }
                            }

                            Rectangle {
                                visible: !monitorItem.internalDisplay
                                width: parent.width * 0.34
                                height: 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: monitorFrame.bottom
                                radius: 3
                                color: Components.Theme.cardBorder
                            }
                            Rectangle {
                                visible: !monitorItem.internalDisplay
                                width: parent.width * 0.58
                                height: 4
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: monitorFrame.bottom
                                anchors.topMargin: 6
                                radius: 2
                                color: Components.Theme.cardBorder
                            }
                            Rectangle {
                                visible: monitorItem.internalDisplay
                                width: parent.width * 0.84
                                height: 5
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: monitorFrame.bottom
                                radius: 2
                                color: Components.Theme.cardBorder
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: root.outputs.length === 0
                        text: root.controller && root.controller.available
                            ? "No displays are currently available"
                            : "Native display information is unavailable"
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeNormal
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Components.Theme.spacingMicro
                    visible: root.selectedOutput !== null

                    Text {
                        objectName: "displayOutputTitle"
                        Layout.fillWidth: true
                        text: root.titleFor(root.selectedOutput)
                        textFormat: Text.PlainText
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeHeader
                        font.weight: Components.Theme.fontWeightDemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.subtitleFor(root.selectedOutput)
                        textFormat: Text.PlainText
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        elide: Text.ElideRight
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.controller && !root.controller.available
                    spacing: Components.Theme.spacingMedium

                    Text {
                        objectName: "displayError"
                        Layout.fillWidth: true
                        text: root.controller && root.controller.lastError !== ""
                            ? root.controller.lastError
                            : "Connect to Typhon to read native display information."
                        textFormat: Text.PlainText
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                    }
                    Controls.Button {
                        objectName: "displayRetry"
                        text: "Retry"
                        flat: true
                        enabled: root.controller && !root.controller.busy
                        onClicked: if (root.controller) root.controller.refresh()
                    }
                }
            }
        }

        Form.SectionHeader {
            text: "RESOLUTION"
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "displayResolutionSection"
            Layout.bottomMargin: 24
            margins: Components.Theme.spacingTiny
            visible: root.selectedOutput !== null

            ColumnLayout {
                width: parent.width
                spacing: 0

                Repeater {
                    model: root.resolutionGroups(root.selectedOutput)
                    delegate: Rectangle {
                        required property int index
                        required property var modelData
                        objectName: "displayResolutionRow_" + modelData.width + "x" + modelData.height
                        Layout.fillWidth: true
                        Layout.preferredHeight: 64
                        color: modelData.current
                            ? Qt.rgba(Components.Theme.accent.r, Components.Theme.accent.g,
                                      Components.Theme.accent.b, Components.Theme.themeMode === 1 ? 0.07 : 0.11)
                            : "transparent"
                        Behavior on color {
                            ColorAnimation { duration: Components.Theme.animationNormal; easing.type: Easing.OutQuart }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Components.Theme.spacingXLarge
                            anchors.rightMargin: Components.Theme.spacingXLarge
                            spacing: Components.Theme.spacingMedium

                            Text {
                                Layout.preferredWidth: 150
                                text: modelData.width + " × " + modelData.height
                                textFormat: Text.PlainText
                                color: modelData.current ? Components.Theme.accent : Components.Theme.textPrimary
                                font.family: Components.Theme.fontFamily
                                font.pixelSize: Components.Theme.fontSizeNormal
                                font.weight: modelData.current ? Components.Theme.fontWeightDemiBold
                                                               : Components.Theme.fontWeightMedium
                            }

                            Rectangle {
                                visible: modelData.preferred
                                objectName: "displayNativeBadge_" + modelData.width + "x" + modelData.height
                                Layout.preferredWidth: nativeText.implicitWidth + Components.Theme.spacingMedium
                                Layout.preferredHeight: 24
                                radius: Components.Theme.controlRadius
                                color: Qt.rgba(Components.Theme.accent.r, Components.Theme.accent.g,
                                               Components.Theme.accent.b, Components.Theme.themeMode === 1 ? 0.10 : 0.18)
                                Text {
                                    id: nativeText
                                    anchors.centerIn: parent
                                    text: "Native"
                                    color: Components.Theme.accent
                                    font.family: Components.Theme.fontFamily
                                    font.pixelSize: Components.Theme.fontSizeTiny
                                    font.weight: Components.Theme.fontWeightDemiBold
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                Layout.maximumWidth: 360
                                objectName: "displayResolutionRates_" + modelData.width + "x" + modelData.height
                                text: {
                                    const values = []
                                    for (let rateIndex = 0; rateIndex < modelData.rates.length; ++rateIndex) {
                                        const rate = modelData.rates[rateIndex]
                                        values.push(root.formatRate(rate.milliHz) + (rate.interlaced ? "i" : "") + " Hz")
                                    }
                                    return values.join(" · ")
                                }
                                textFormat: Text.PlainText
                                color: Components.Theme.textSecondary
                                font.family: Components.Theme.fontFamily
                                font.pixelSize: Components.Theme.fontSizeSmall
                                horizontalAlignment: Text.AlignRight
                                wrapMode: Text.Wrap
                            }
                        }

                        Rectangle {
                            visible: index < root.resolutionGroups(root.selectedOutput).length - 1
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: Components.Theme.spacingXLarge
                            height: 1
                            color: Components.Theme.cardBorder
                        }
                    }
                }

                Text {
                    objectName: "displayNoModes"
                    Layout.fillWidth: true
                    Layout.margins: Components.Theme.spacingXLarge
                    visible: root.selectedOutput && root.resolutionGroups(root.selectedOutput).length === 0
                    text: root.selectedOutput && root.selectedOutput.hasCurrentMode
                        ? "No additional mode information is available."
                        : "No current display mode is active."
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    textFormat: Text.PlainText
                }
                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: Components.Theme.spacingXLarge
                    Layout.rightMargin: Components.Theme.spacingXLarge
                    Layout.bottomMargin: Components.Theme.spacingMedium
                    visible: root.selectedOutput && root.selectedOutput.modesTruncated
                    text: "Some available modes are not shown."
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeTiny
                    textFormat: Text.PlainText
                }
            }
        }

        Form.SectionHeader {
            text: "DISPLAY DETAILS"
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "displayDetailsCard"
            margins: Components.Theme.spacingTiny
            visible: root.selectedOutput !== null

            Form.SettingRow {
                label: "Refresh rate"
                sublabel: "Current mode refresh rate"
                Text {
                    objectName: "displayRefreshRate"
                    text: root.selectedOutput && root.selectedOutput.hasCurrentMode
                        ? root.formatRate(root.selectedOutput.currentRefreshMilliHz) + " Hz" : "Unavailable"
                    textFormat: Text.PlainText
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeNormal
                }
            }
            Form.SettingRow {
                label: "Display scale"
                sublabel: "Current logical scale"
                Text {
                    objectName: "displayScale"
                    text: root.selectedOutput ? root.formatScale(root.selectedOutput.scaleMilli) : "Unavailable"
                    textFormat: Text.PlainText
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeNormal
                }
            }
            Form.SettingRow {
                label: "Orientation"
                sublabel: "Current display orientation"
                Text {
                    objectName: "displayOrientation"
                    text: root.selectedOutput ? root.orientationText(root.selectedOutput.transform) : "Unavailable"
                    textFormat: Text.PlainText
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeNormal
                }
            }
            Form.SettingRow {
                label: "Variable refresh rate"
                sublabel: "Reported connector capability"
                Text {
                    objectName: "displayVrrState"
                    text: root.selectedOutput ? root.vrrText(root.selectedOutput.vrrState) : "Unavailable"
                    textFormat: Text.PlainText
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeNormal
                }
            }
            Form.SettingRow {
                label: "Connector"
                sublabel: "Physical output connection"
                isLast: true
                Text {
                    objectName: "displayConnector"
                    text: root.selectedOutput && root.nonEmpty(root.selectedOutput.name)
                        ? String(root.selectedOutput.name) : "Unavailable"
                    textFormat: Text.PlainText
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeNormal
                }
            }
        }
    }
}
