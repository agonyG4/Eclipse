import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
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
    readonly property var wallpaper:
        typeof SettingsController !== "undefined" && SettingsController
            ? SettingsController.wallpaper : null

    property string selectedOutputId: ""
    property bool showAllResolutions: false

    readonly property var selectedOutput: {
        for (let index = 0; index < root.outputs.length; ++index) {
            if (String(root.outputs[index].id) === root.selectedOutputId)
                return root.outputs[index]
        }
        return null
    }
    readonly property int selectedIndex: {
        for (let index = 0; index < root.outputs.length; ++index) {
            if (String(root.outputs[index].id) === root.selectedOutputId)
                return index
        }
        return -1
    }
    readonly property bool backendUnavailable:
        !Boolean(root.controller) || !Boolean(root.controller.available)
    readonly property bool refreshFailed:
        Boolean(root.controller && root.nonEmpty(root.controller.lastError))
    readonly property bool loading:
        Boolean(root.controller && Boolean(root.controller.busy)
            && root.outputs.length === 0 && !root.refreshFailed)
    readonly property bool noOutputs:
        Boolean(!root.backendUnavailable && !root.refreshFailed && !root.loading
            && root.outputs.length === 0)
    readonly property bool showDisplayContent:
        Boolean(!root.backendUnavailable && !root.refreshFailed
            && root.selectedOutput !== null)
    readonly property var allResolutionGroups:
        root.resolutionGroups(root.selectedOutput)
    readonly property var defaultResolutionGroups:
        root.defaultResolutionSubset(root.allResolutionGroups)
    readonly property var visibleResolutionGroups:
        root.showAllResolutions
            ? root.allResolutionGroups : root.defaultResolutionGroups
    readonly property var currentResolutionGroup: {
        for (let index = 0; index < root.allResolutionGroups.length; ++index) {
            if (root.allResolutionGroups[index].current)
                return root.allResolutionGroups[index]
        }
        return null
    }

    onSelectedOutputIdChanged: root.showAllResolutions = false

    function nonEmpty(value) {
        return value !== undefined && value !== null
            && String(value).trim().length > 0
    }

    function isBuiltIn(output) {
        const name = output && root.nonEmpty(output.name)
            ? String(output.name).toLowerCase() : ""
        return name.startsWith("edp") || name.startsWith("lvds")
    }

    function titleFor(output) {
        if (!output)
            return "Display"
        if (root.nonEmpty(output.model))
            return String(output.model).trim()
        if (root.nonEmpty(output.make))
            return String(output.make).trim()
        if (root.nonEmpty(output.name))
            return String(output.name)
        return root.isBuiltIn(output) ? "Laptop Display" : "Display"
    }

    function kindFor(output) {
        return root.isBuiltIn(output) ? "Built-in display" : "External display"
    }

    function subtitleFor(output) {
        if (!output)
            return ""
        const pieces = [root.kindFor(output)]
        const make = root.nonEmpty(output.make) ? String(output.make).trim() : ""
        if (make !== "" && make !== root.titleFor(output))
            pieces.push(make)
        if (root.nonEmpty(output.name))
            pieces.push(String(output.name))
        return pieces.join(" · ")
    }

    function monitorSelectorLabel(output) {
        if (!output)
            return "Display"
        const title = root.titleFor(output)
        const connector = root.nonEmpty(output.name) ? String(output.name) : ""
        return connector !== "" && connector !== title
            ? title + " · " + connector : title
    }

    function isQuarterTurn(output) {
        const transform = output && output.transform !== undefined
            ? String(output.transform) : "normal"
        return transform === "90" || transform === "270"
            || transform === "flipped_90" || transform === "flipped_270"
    }

    function outputWidth(output) {
        if (!output || !Boolean(output.hasCurrentMode))
            return 240
        const width = Number(output.currentWidth)
        const height = Number(output.currentHeight)
        if (width <= 0 || height <= 0)
            return 240
        return root.isQuarterTurn(output) ? height : width
    }

    function outputHeight(output) {
        if (!output || !Boolean(output.hasCurrentMode))
            return 135
        const width = Number(output.currentWidth)
        const height = Number(output.currentHeight)
        if (width <= 0 || height <= 0)
            return 135
        return root.isQuarterTurn(output) ? width : height
    }

    function boundsForOutputs() {
        if (root.outputs.length === 0)
            return { minX: 0, minY: 0, width: 1920, height: 1080 }

        let minX = Number(root.outputs[0].x) || 0
        let minY = Number(root.outputs[0].y) || 0
        let maxX = minX + root.outputWidth(root.outputs[0])
        let maxY = minY + root.outputHeight(root.outputs[0])
        for (let index = 1; index < root.outputs.length; ++index) {
            const output = root.outputs[index]
            const x = Number(output.x) || 0
            const y = Number(output.y) || 0
            minX = Math.min(minX, x)
            minY = Math.min(minY, y)
            maxX = Math.max(maxX, x + root.outputWidth(output))
            maxY = Math.max(maxY, y + root.outputHeight(output))
        }
        return {
            minX: minX,
            minY: minY,
            width: Math.max(1, maxX - minX),
            height: Math.max(1, maxY - minY)
        }
    }

    function reconcileSelection() {
        for (let index = 0; index < root.outputs.length; ++index) {
            if (String(root.outputs[index].id) === root.selectedOutputId)
                return
        }
        for (let index = 0; index < root.outputs.length; ++index) {
            if (root.outputs[index].focused) {
                root.selectedOutputId = String(root.outputs[index].id)
                return
            }
        }
        root.selectedOutputId = root.outputs.length > 0
            ? String(root.outputs[0].id) : ""
    }

    function resolutionKey(group) {
        return String(group.width) + "x" + String(group.height)
    }

    function formatResolution(group) {
        return String(group.width) + " × " + String(group.height)
    }

    function formatRate(milliHz) {
        const hz = Number(milliHz) / 1000
        if (!Number.isFinite(hz) || hz <= 0)
            return "Unavailable"
        return String(Number(hz.toFixed(3)))
    }

    function formatScale(scaleMilli) {
        const percent = Number(scaleMilli) / 10
        if (!Number.isFinite(percent) || percent <= 0)
            return "Unavailable"
        return (Number.isInteger(percent)
            ? String(percent) : String(Number(percent.toFixed(1)))) + "%"
    }

    function currentMode(output) {
        if (!output || !Boolean(output.hasCurrentMode) || !output.modes)
            return null
        return root.modeById(output, output.currentModeId)
    }

    function modeById(output, modeId) {
        if (!output || !output.modes || modeId === undefined || modeId === null)
            return null
        const requestedId = String(modeId)
        for (let index = 0; index < output.modes.length; ++index) {
            if (String(output.modes[index].id) === requestedId)
                return output.modes[index]
        }
        return null
    }

    function selectedMode(output) {
        if (!output)
            return null
        const draft = Boolean(output.draftPresent)
            ? root.modeById(output, output.draftModeId) : null
        return draft || root.currentMode(output)
    }

    function selectedScaleMilli(output) {
        if (!output)
            return 0
        const draftScale = Number(output.draftScaleMilli)
        return Boolean(output.draftPresent) && draftScale > 0
            ? draftScale : Number(output.scaleMilli)
    }

    function selectedTransform(output) {
        if (!output)
            return "normal"
        return Boolean(output.draftPresent)
            ? String(output.draftTransform) : String(output.transform)
    }

    function rateText(mode) {
        if (!mode)
            return "Unavailable"
        const rate = root.formatRate(mode.refreshMilliHz)
        return rate === "Unavailable" ? rate
            : rate + " Hz" + (Boolean(mode.interlaced) ? " (interlaced)" : "")
    }

    function refreshLabel(output) {
        if (!output || !Boolean(output.hasCurrentMode))
            return "Unavailable"
        const rate = root.formatRate(output.currentRefreshMilliHz)
        if (rate === "Unavailable")
            return rate
        const mode = root.currentMode(output)
        return rate + " Hz" + (mode && mode.interlaced ? " (interlaced)" : "")
    }

    function selectedRefreshLabel(output) {
        if (!output)
            return "Unavailable"
        const mode = root.selectedMode(output)
        return mode ? root.rateText(mode) : root.refreshLabel(output)
    }

    function modeSummary(output) {
        if (!output || !Boolean(output.hasCurrentMode))
            return "No current mode"
        return String(output.currentWidth) + " × " + String(output.currentHeight)
            + " • " + root.refreshLabel(output)
    }

    function vrrText(state) {
        switch (String(state)) {
        case "active": return "Active"
        case "available":
        case "configured": return "Supported"
        case "unavailable": return "Not supported"
        case "degraded": return "Unavailable"
        default: return "Unavailable"
        }
    }

    function resolutionGroups(output) {
        if (!output || !output.modes)
            return []

        const selectionMode = root.selectedMode(output)
        const selectionWidth = selectionMode
            ? Number(selectionMode.width) : Number(output.currentWidth)
        const selectionHeight = selectionMode
            ? Number(selectionMode.height) : Number(output.currentHeight)

        const bySize = Object.create(null)
        for (let index = 0; index < output.modes.length; ++index) {
            const mode = output.modes[index]
            const width = Number(mode.width)
            const height = Number(mode.height)
            if (width <= 0 || height <= 0)
                continue
            const key = String(width) + "x" + String(height)
            if (!bySize[key]) {
                bySize[key] = {
                    width: width,
                    height: height,
                    preferred: false,
                    current: (selectionMode !== null || Boolean(output.hasCurrentMode))
                        && selectionWidth === width
                        && selectionHeight === height,
                    rates: []
                }
            }
            const group = bySize[key]
            group.preferred = group.preferred || Boolean(mode.preferred)
            group.rates.push({
                id: mode.id,
                refreshMilliHz: Number(mode.refreshMilliHz),
                interlaced: Boolean(mode.interlaced),
                preferred: Boolean(mode.preferred)
            })
        }

        const groups = Object.keys(bySize).map(key => bySize[key])
        if (Boolean(output.hasCurrentMode)) {
            const currentKey = String(Number(output.currentWidth)) + "x"
                + String(Number(output.currentHeight))
            if (!bySize[currentKey] && Number(output.currentWidth) > 0
                    && Number(output.currentHeight) > 0) {
                groups.push({
                    width: Number(output.currentWidth),
                    height: Number(output.currentHeight),
                    preferred: false,
                    current: true,
                    rates: []
                })
            }
        }

        groups.sort((left, right) =>
            right.width * right.height - left.width * left.height
                || right.width - left.width || right.height - left.height)
        for (let groupIndex = 0; groupIndex < groups.length; ++groupIndex) {
            groups[groupIndex].rates.sort((left, right) =>
                left.refreshMilliHz - right.refreshMilliHz
                    || Number(left.interlaced) - Number(right.interlaced)
                    || Number(left.id) - Number(right.id))
        }
        return groups
    }

    function modeForResolution(group, output) {
        if (!group || !group.rates || group.rates.length === 0)
            return null

        const current = root.selectedMode(output)
        if (current) {
            const matching = group.rates.filter(mode =>
                Number(mode.refreshMilliHz) === Number(current.refreshMilliHz)
                    && Boolean(mode.interlaced) === Boolean(current.interlaced))
            if (matching.length > 0)
                return matching.sort((left, right) =>
                    Number(left.id) - Number(right.id))[0]
        }

        const preferred = group.rates.filter(mode => Boolean(mode.preferred))
        const candidates = preferred.length > 0 ? preferred : group.rates
        const preservingInterlace = current
            ? candidates.filter(mode =>
                Boolean(mode.interlaced) === Boolean(current.interlaced)) : []
        const ranked = (preservingInterlace.length > 0
            ? preservingInterlace : candidates).slice().sort((left, right) =>
                Number(right.refreshMilliHz) - Number(left.refreshMilliHz)
                    || Number(left.interlaced) - Number(right.interlaced)
                    || Number(left.id) - Number(right.id))
        return ranked.length > 0 ? ranked[0] : null
    }

    function setDraftForMode(output, mode, scaleMilli, transform) {
        if (!root.controller || !output || !mode)
            return false
        return root.controller.setDraft(String(output.id), Number(mode.id),
            Number(scaleMilli), String(transform))
    }

    function selectResolution(group) {
        const output = root.selectedOutput
        if (!output || output.modeSelectionSupported !== true)
            return
        const mode = root.modeForResolution(group, output)
        if (!mode)
            return
        root.setDraftForMode(output, mode, root.selectedScaleMilli(output),
            root.selectedTransform(output))
    }

    function selectedRefreshIndex(output, group) {
        if (!output || !group || !group.rates)
            return -1
        const selected = root.selectedMode(output)
        if (!selected)
            return -1
        return group.rates.findIndex(mode => String(mode.id) === String(selected.id))
    }

    function selectRefresh(index) {
        const output = root.selectedOutput
        const group = root.currentResolutionGroup
        if (!output || output.modeSelectionSupported !== true || !group
                || index < 0 || index >= group.rates.length)
            return
        root.setDraftForMode(output, group.rates[index],
            root.selectedScaleMilli(output), root.selectedTransform(output))
    }

    function scaleValues(output) {
        if (!output || !output.scaleValuesMilli)
            return []
        return output.scaleValuesMilli.map(value => Number(value))
    }

    function scaleLabels(output) {
        return root.scaleValues(output).map(value => root.formatScale(value))
    }

    function selectedScaleIndex(output) {
        const values = root.scaleValues(output)
        const selected = root.selectedScaleMilli(output)
        return values.findIndex(value => Number(value) === selected)
    }

    function selectScale(index) {
        const output = root.selectedOutput
        const values = root.scaleValues(output)
        if (!output || output.scaleMutationSupported !== true
                || index < 0 || index >= values.length)
            return
        const mode = root.selectedMode(output)
        if (!mode)
            return
        root.setDraftForMode(output, mode, values[index],
            root.selectedTransform(output))
    }

    function defaultResolutionSubset(groups) {
        if (!groups || groups.length <= 5)
            return groups || []

        const selected = []
        const add = group => {
            if (group && selected.length < 5
                    && !selected.some(item => root.resolutionKey(item)
                        === root.resolutionKey(group)))
                selected.push(group)
        }
        const current = groups.find(group => group.current)
        const preferred = groups.find(group => group.preferred)
        add(current)
        add(preferred)
        add(groups[0])

        const commonSizes = ["1600x900", "1366x768", "1280x720", "1024x768"]
        for (let index = 0; index < commonSizes.length && selected.length < 5; ++index)
            add(groups.find(group => root.resolutionKey(group) === commonSizes[index]))
        for (let index = 0; index < groups.length && selected.length < 5; ++index)
            add(groups[index])
        selected.sort((left, right) =>
            right.width * right.height - left.width * left.height
                || right.width - left.width || right.height - left.height)
        return selected
    }

    Component.onCompleted: {
        root.reconcileSelection()
        if (root.controller)
            root.controller.refresh()
        if (root.wallpaper && !root.wallpaper.busy)
            root.wallpaper.refresh()
    }

    Connections {
        target: root.controller
        function onSnapshotChanged() { root.reconcileSelection() }
    }

    component DisplaySelector: Controls.SelectButton {
        property int controlWidth: 130
        width: controlWidth
        implicitWidth: controlWidth
        implicitHeight: 36
    }

    component DisplayPreview: Item {
        id: preview
        objectName: "displayOutputPreview"
        implicitHeight: 252
        height: 252

        readonly property var bounds: root.boundsForOutputs()
        readonly property real previewScale: Math.min(
            width / Math.max(1, bounds.width),
            height / Math.max(1, bounds.height)
        ) * 0.78
        property bool previewReady: false

        Component.onCompleted: preview.previewReady = true

        Repeater {
            model: root.outputs

            delegate: Item {
                id: monitorItem
                required property int index
                required property var modelData
                objectName: "displayPreviewMonitor_" + String(modelData.id)

                readonly property bool active:
                    String(modelData.id) === root.selectedOutputId
                readonly property bool laptop: root.isBuiltIn(modelData)
                readonly property bool hasCurrentMode:
                    Boolean(modelData.hasCurrentMode)
                readonly property real screenWidth:
                    Math.max(56, root.outputWidth(modelData) * preview.previewScale)
                readonly property real screenHeight:
                    Math.max(36, root.outputHeight(modelData) * preview.previewScale)
                readonly property real bezel: laptop ? 12 : 10
                readonly property real chassisHeight: laptop ? 16 : 38

                width: screenWidth + bezel * 2
                height: screenHeight + bezel * 2 + chassisHeight
                x: (preview.width - preview.bounds.width * preview.previewScale) / 2
                    + ((Number(modelData.x) - preview.bounds.minX)
                       * preview.previewScale) - bezel
                y: (preview.height - preview.bounds.height * preview.previewScale) / 2
                    + ((Number(modelData.y) - preview.bounds.minY)
                       * preview.previewScale) - bezel

                Behavior on x {
                    enabled: preview.previewReady
                    NumberAnimation {
                        duration: Components.Theme.animationNormal
                        easing.type: Easing.OutCubic
                    }
                }
                Behavior on y {
                    enabled: preview.previewReady
                    NumberAnimation {
                        duration: Components.Theme.animationNormal
                        easing.type: Easing.OutCubic
                    }
                }
                Behavior on width {
                    enabled: preview.previewReady
                    NumberAnimation {
                        duration: Components.Theme.animationNormal
                        easing.type: Easing.OutCubic
                    }
                }
                Behavior on height {
                    enabled: preview.previewReady
                    NumberAnimation {
                        duration: Components.Theme.animationNormal
                        easing.type: Easing.OutCubic
                    }
                }

                Rectangle {
                    anchors.fill: screenFrame
                    anchors.margins: -3
                    radius: screenFrame.radius + 3
                    color: "transparent"
                    border.width: monitorItem.active ? 1 : 0
                    border.color: Qt.rgba(Components.Theme.accent.r,
                                          Components.Theme.accent.g,
                                          Components.Theme.accent.b, 0.28)
                }

                Rectangle {
                    id: screenFrame
                    x: monitorItem.bezel
                    y: monitorItem.bezel
                    width: monitorItem.screenWidth
                    height: monitorItem.screenHeight
                    radius: monitorItem.laptop ? 10 : 9
                    color: Components.Theme.cardBg
                    border.width: 1
                    border.color: monitorItem.active
                        ? Qt.rgba(Components.Theme.accent.r,
                                  Components.Theme.accent.g,
                                  Components.Theme.accent.b, 0.72)
                        : Components.Theme.cardBorder
                    clip: true

                    Behavior on width {
                        enabled: preview.previewReady
                        NumberAnimation { duration: Components.Theme.animationNormal }
                    }
                    Behavior on height {
                        enabled: preview.previewReady
                        NumberAnimation { duration: Components.Theme.animationNormal }
                    }
                    Behavior on border.color {
                        ColorAnimation { duration: Components.Theme.animationNormal }
                    }

                    Rectangle {
                        id: screenSurface
                        anchors.fill: parent
                        anchors.margins: monitorItem.laptop ? 8 : 7
                        radius: Math.max(4, screenFrame.radius - 4)
                        color: Components.Theme.windowBackground
                        clip: true

                        Rectangle {
                            objectName: "displayWallpaperFallback_" + String(modelData.id)
                            anchors.fill: parent
                            visible: !modelData.enabled || !monitorItem.hasCurrentMode
                                || !wallpaperImage.hasUsableSource
                                || wallpaperImage.status === Image.Error
                            radius: screenSurface.radius
                            gradient: Gradient {
                                GradientStop {
                                    position: 0
                                    color: Components.Theme.cardBg
                                }
                                GradientStop {
                                    position: 1
                                    color: Qt.darker(Components.Theme.accent, 1.25)
                                }
                            }
                        }

                        Image {
                            id: wallpaperImage
                            objectName: "displayWallpaperPreview"
                            readonly property bool hasUsableSource:
                                Boolean(root.wallpaper)
                                    && root.nonEmpty(root.wallpaper.effectivePreviewUrl)
                            anchors.fill: parent
                            visible: Boolean(modelData.enabled)
                                && monitorItem.hasCurrentMode
                                && hasUsableSource && status !== Image.Error
                            source: root.wallpaper
                                ? root.wallpaper.effectivePreviewUrl : ""
                            fillMode: {
                                if (!root.wallpaper
                                        || root.wallpaper.effectiveFit === "cover")
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
                            retainWhileLoading: true
                            smooth: true
                            mipmap: true
                            sourceSize.width: Math.max(1,
                                Math.ceil(width * Screen.devicePixelRatio))
                            sourceSize.height: Math.max(1,
                                Math.ceil(height * Screen.devicePixelRatio))
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: screenSurface.radius
                            color: Qt.rgba(Components.Theme.windowBackground.r,
                                           Components.Theme.windowBackground.g,
                                           Components.Theme.windowBackground.b, 0.08)
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !modelData.enabled || !monitorItem.hasCurrentMode
                            text: modelData.enabled ? "No current mode" : "Disabled"
                            textFormat: Text.PlainText
                            color: Components.Theme.textSecondary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeSmall
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 24
                            color: Qt.rgba(Components.Theme.windowBackground.r,
                                           Components.Theme.windowBackground.g,
                                           Components.Theme.windowBackground.b, 0.62)
                            Text {
                                objectName: "displayConnector"
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: Components.Theme.spacingSmall
                                anchors.rightMargin: Components.Theme.spacingSmall
                                text: root.nonEmpty(modelData.name)
                                    ? String(modelData.name) : "Display"
                                textFormat: Text.PlainText
                                color: Components.Theme.textPrimary
                                font.family: Components.Theme.fontFamily
                                font.pixelSize: Components.Theme.fontSizeTiny
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
                    objectName: "displayMonitorStand_" + String(modelData.id)
                    visible: !monitorItem.laptop
                    anchors.horizontalCenter: screenFrame.horizontalCenter
                    y: screenFrame.y + screenFrame.height + 1
                    width: Math.max(13, screenFrame.width * 0.075)
                    height: 22
                    radius: 4
                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: Qt.rgba(Components.Theme.textPrimary.r,
                                           Components.Theme.textPrimary.g,
                                           Components.Theme.textPrimary.b, 0.22)
                        }
                        GradientStop {
                            position: 1
                            color: Qt.rgba(Components.Theme.textPrimary.r,
                                           Components.Theme.textPrimary.g,
                                           Components.Theme.textPrimary.b, 0.08)
                        }
                    }
                }

                Rectangle {
                    objectName: "displayMonitorStandBase_" + String(modelData.id)
                    visible: !monitorItem.laptop
                    anchors.horizontalCenter: screenFrame.horizontalCenter
                    y: screenFrame.y + screenFrame.height + 23
                    width: Math.max(64, screenFrame.width * 0.34)
                    height: 6
                    radius: 4
                    color: Qt.rgba(Components.Theme.textPrimary.r,
                                   Components.Theme.textPrimary.g,
                                   Components.Theme.textPrimary.b, 0.16)
                }

                Rectangle {
                    objectName: "displayLaptopChassis_" + String(modelData.id)
                    visible: monitorItem.laptop
                    anchors.horizontalCenter: screenFrame.horizontalCenter
                    y: screenFrame.y + screenFrame.height + 3
                    width: screenFrame.width + 24
                    height: 10
                    radius: 5
                    color: Qt.rgba(Components.Theme.textPrimary.r,
                                   Components.Theme.textPrimary.g,
                                   Components.Theme.textPrimary.b, 0.17)

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.max(24, parent.width * 0.18)
                        height: 3
                        radius: 2
                        color: Qt.rgba(Components.Theme.windowBackground.r,
                                       Components.Theme.windowBackground.g,
                                       Components.Theme.windowBackground.b, 0.24)
                    }
                }
            }
        }
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 28
        maxWidth: 1000

        ColumnLayout {
            id: pageContents
            objectName: "displayPageContent"
            Layout.fillWidth: true
            width: parent.width
            spacing: 0

            Form.FormCard {
                objectName: "displayStatusCard"
                visible: root.backendUnavailable || root.refreshFailed
                    || root.loading || root.noOutputs
                Layout.bottomMargin: 24
                radius: 12
                margins: Components.Theme.spacingLarge

                ColumnLayout {
                    width: parent.width
                    spacing: Components.Theme.spacingMedium

                    Text {
                        objectName: "displayLoading"
                        visible: root.loading
                        Layout.fillWidth: true
                        text: "Loading display information…"
                        textFormat: Text.PlainText
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeNormal
                    }
                    Text {
                        objectName: "displayBackendUnavailable"
                        visible: root.backendUnavailable
                        Layout.fillWidth: true
                        text: "Display service is unavailable."
                        textFormat: Text.PlainText
                        color: Components.Theme.warningColor
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeNormal
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        objectName: "displayRefreshError"
                        visible: root.refreshFailed
                        Layout.fillWidth: true
                        text: root.controller ? String(root.controller.lastError) : ""
                        textFormat: Text.PlainText
                        color: Components.Theme.errorColor
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeNormal
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        objectName: "displayNoOutputs"
                        visible: root.noOutputs
                        Layout.fillWidth: true
                        text: "No displays are connected."
                        textFormat: Text.PlainText
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeNormal
                    }
                    Controls.Button {
                        objectName: "displayRetryButton"
                        visible: Boolean(root.controller)
                            && (root.backendUnavailable || root.refreshFailed
                                || root.noOutputs)
                        Layout.alignment: Qt.AlignRight
                        text: "Retry"
                        enabled: Boolean(root.controller)
                        onClicked: {
                            if (root.controller)
                                root.controller.refresh()
                        }
                    }
                }
            }

            ColumnLayout {
                id: displayContent
                objectName: "displayContent"
                visible: root.showDisplayContent
                Layout.fillWidth: true
                width: pageContents.width
                spacing: 0

                ColumnLayout {
                    id: previewColumn
                    visible: root.selectedOutput !== null
                    objectName: "displayPreviewSection"
                    Layout.fillWidth: true
                    width: displayContent.width
                    Layout.bottomMargin: 24
                    spacing: Components.Theme.spacingLarge

                    Form.SectionHeader {
                        objectName: "displayPreviewSectionHeader"
                        Layout.fillWidth: true
                        text: "DISPLAY PREVIEW"
                    }

                    Item {
                        id: previewOpenArea
                        objectName: "displayPreviewOpenArea"
                        Layout.fillWidth: true
                        implicitHeight: previewDevice.implicitHeight + 24

                        Column {
                            id: previewDevice
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            width: Math.min(620, previewColumn.width)
                            spacing: 14

                            DisplayPreview { width: previewDevice.width }

                            Column {
                                id: previewIdentity
                                objectName: "displayPreviewIdentity"
                                width: parent.width
                                spacing: 5

                                Text {
                                    objectName: "displayOutputTitle"
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    text: root.titleFor(root.selectedOutput)
                                    textFormat: Text.PlainText
                                    color: Components.Theme.textPrimary
                                    font.family: Components.Theme.fontFamily
                                    font.pixelSize: 22
                                    font.weight: Components.Theme.fontWeightDemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    objectName: "displayOutputSubtitle"
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    text: root.subtitleFor(root.selectedOutput)
                                    textFormat: Text.PlainText
                                    color: Components.Theme.textSecondary
                                    font.family: Components.Theme.fontFamily
                                    font.pixelSize: Components.Theme.fontSizeSmall
                                    elide: Text.ElideRight
                                }
                                Text {
                                    objectName: "displayModeSummary"
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    text: root.modeSummary(root.selectedOutput)
                                    textFormat: Text.PlainText
                                    color: Components.Theme.textTertiary
                                    font.family: Components.Theme.fontFamily
                                    font.pixelSize: Components.Theme.fontSizeSmall
                                }
                            }
                        }
                    }
                }

                Text {
                    objectName: "displayOutputsTruncated"
                    visible: Boolean(root.controller && Boolean(root.controller.truncated))
                    Layout.fillWidth: true
                    Layout.bottomMargin: 12
                    text: "Additional displays are not shown."
                    textFormat: Text.PlainText
                    color: Components.Theme.warningColor
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                ColumnLayout {
                    objectName: "displayMonitorSection"
                    visible: root.outputs.length > 1
                    Layout.fillWidth: true
                    Layout.bottomMargin: 24
                    spacing: Components.Theme.spacingSmall

                    Form.SectionHeader {
                        objectName: "displayMonitorSectionHeader"
                        Layout.fillWidth: true
                        text: "MONITOR"
                    }

                    ComboBox {
                        id: monitorSelector
                        objectName: "displayMonitorSelector"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        model: root.outputs
                        textRole: "name"
                        currentIndex: root.selectedIndex
                        displayText: root.monitorSelectorLabel(root.selectedOutput)
                        onActivated: index => {
                            if (index >= 0 && index < root.outputs.length)
                                root.selectedOutputId = String(root.outputs[index].id)
                        }

                        background: Rectangle {
                            radius: Components.Theme.controlRadius
                            color: Components.Theme.cardBg
                            border.width: 1
                            border.color: monitorSelector.popup.visible
                                ? Components.Theme.accent : Components.Theme.cardBorder
                        }
                        contentItem: Text {
                            leftPadding: Components.Theme.spacingMedium
                            rightPadding: monitorSelector.indicator.width
                                + Components.Theme.spacingLarge
                            text: root.monitorSelectorLabel(root.selectedOutput)
                            textFormat: Text.PlainText
                            color: Components.Theme.textPrimary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeNormal
                            font.weight: Components.Theme.fontWeightMedium
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                        indicator: Text {
                            x: monitorSelector.width - width
                            width: 36
                            height: monitorSelector.height
                            text: monitorSelector.popup.visible ? "⌃" : "⌄"
                            textFormat: Text.PlainText
                            color: Components.Theme.textSecondary
                            font.pixelSize: Components.Theme.fontSizeNormal
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        delegate: ItemDelegate {
                            required property int index
                            required property var modelData
                            width: monitorSelector.width
                            contentItem: Text {
                                text: root.monitorSelectorLabel(modelData)
                                textFormat: Text.PlainText
                                color: index === root.selectedIndex
                                    ? Components.Theme.accent
                                    : Components.Theme.textPrimary
                                font.family: Components.Theme.fontFamily
                                font.pixelSize: Components.Theme.fontSizeNormal
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

                Form.SectionHeader {
                    objectName: "displayResolutionSectionHeader"
                    Layout.fillWidth: true
                    Layout.bottomMargin: 8
                    text: "RESOLUTION"
                }

                ColumnLayout {
                    objectName: "displayResolutionList"
                    Layout.fillWidth: true
                    Layout.bottomMargin: 24
                    spacing: 2

                    Repeater {
                        model: root.visibleResolutionGroups

                        delegate: Rectangle {
                            required property var modelData
                            readonly property bool selected: Boolean(modelData.current)
                            readonly property bool interactive:
                                Boolean(root.selectedOutput)
                                    && root.selectedOutput.modeSelectionSupported === true
                                    && modelData.rates.length > 0
                            readonly property string key: root.resolutionKey(modelData)

                            objectName: "displayResolutionRow_" + key
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            implicitHeight: 36
                            radius: 8
                            color: selected
                                ? Qt.rgba(Components.Theme.accent.r,
                                          Components.Theme.accent.g,
                                          Components.Theme.accent.b, 0.12)
                                : (interactive && resolutionActivation.containsMouse
                                    ? Qt.rgba(Components.Theme.accent.r,
                                              Components.Theme.accent.g,
                                              Components.Theme.accent.b, 0.06)
                                    : "transparent")
                            border.width: selected || (interactive && activeFocus) ? 1 : 0
                            border.color: selected || (interactive && activeFocus)
                                ? Components.Theme.accent : "transparent"
                            activeFocusOnTab: interactive
                            Accessible.role: Accessible.Button
                            Accessible.name: root.formatResolution(modelData)

                            Keys.onPressed: function(event) {
                                if (interactive && (event.key === Qt.Key_Space
                                        || event.key === Qt.Key_Return
                                        || event.key === Qt.Key_Enter)) {
                                    root.selectResolution(modelData)
                                    event.accepted = true
                                }
                            }

                            Behavior on color {
                                ColorAnimation { duration: Components.Theme.animationQuick }
                            }
                            Behavior on border.color {
                                ColorAnimation { duration: Components.Theme.animationQuick }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: Components.Theme.spacingSmall

                                Text {
                                    objectName: "displayResolutionLabel_" + key
                                    text: root.formatResolution(modelData)
                                    textFormat: Text.PlainText
                                    color: selected
                                        ? Components.Theme.accent
                                        : Components.Theme.textPrimary
                                    font.family: Components.Theme.fontFamily
                                    font.pixelSize: Components.Theme.fontSizeNormal
                                    font.weight: selected
                                        ? Components.Theme.fontWeightMedium
                                        : Components.Theme.fontWeightNormal
                                }

                                Rectangle {
                                    objectName: "displayPreferredBadge_" + key
                                    visible: Boolean(modelData.preferred)
                                    implicitWidth: preferredLabel.implicitWidth + 10
                                    implicitHeight: 16
                                    radius: 4
                                    color: selected
                                        ? Qt.rgba(Components.Theme.accent.r,
                                                  Components.Theme.accent.g,
                                                  Components.Theme.accent.b, 0.22)
                                        : Qt.rgba(Components.Theme.textSecondary.r,
                                                  Components.Theme.textSecondary.g,
                                                  Components.Theme.textSecondary.b, 0.1)

                                    Text {
                                        id: preferredLabel
                                        objectName: "displayPreferredLabel_" + key
                                        anchors.centerIn: parent
                                        text: "Preferred"
                                        textFormat: Text.PlainText
                                        color: selected
                                            ? Components.Theme.accent
                                            : Components.Theme.textSecondary
                                        font.family: Components.Theme.fontFamily
                                        font.pixelSize: Components.Theme.fontSizeTiny
                                        font.weight: Components.Theme.fontWeightMedium
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    objectName: "displayResolutionCheck_" + key
                                    visible: selected
                                    text: "✓"
                                    textFormat: Text.PlainText
                                    color: Components.Theme.accent
                                    font.pixelSize: Components.Theme.fontSizeNormal
                                    font.weight: Components.Theme.fontWeightBold
                                }
                            }

                            MouseArea {
                                id: resolutionActivation
                                objectName: "displayResolutionActivation_" + key
                                anchors.fill: parent
                                enabled: interactive
                                hoverEnabled: interactive
                                cursorShape: interactive
                                    ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onPressed: parent.forceActiveFocus()
                                onClicked: root.selectResolution(modelData)
                            }
                        }
                    }

                    Text {
                        objectName: "displayResolutionListToggleLabel"
                        visible: root.allResolutionGroups.length
                            > root.defaultResolutionGroups.length
                        Layout.topMargin: 6
                        text: root.showAllResolutions
                            ? "⌃  Show less" : "⌄  Show all resolutions"
                        textFormat: Text.PlainText
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        font.weight: Components.Theme.fontWeightMedium

                        MouseArea {
                            objectName: "displayResolutionListToggle"
                            anchors.fill: parent
                            anchors.margins: -4
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.showAllResolutions = !root.showAllResolutions
                        }
                    }

                    Text {
                        objectName: "displayModesTruncated"
                        visible: Boolean(root.selectedOutput
                            && Boolean(root.selectedOutput.modesTruncated))
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        text: "Some available modes are not shown."
                        textFormat: Text.PlainText
                        color: Components.Theme.warningColor
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                    }
                }

                Form.FormCard {
                    objectName: "displaySettingsCard"
                    Layout.bottomMargin: 24
                    radius: 12
                    margins: 0

                    ColumnLayout {
                        width: parent.width
                        spacing: 0

                        Form.SettingRow {
                            objectName: "displaySettingRefresh"
                            label: "Refresh rate"
                            isLast: false

                            DisplaySelector {
                                objectName: "displayRefreshControl"
                                controlWidth: 130
                                label: root.selectedRefreshLabel(root.selectedOutput)
                                options: root.currentResolutionGroup
                                    ? root.currentResolutionGroup.rates.map(
                                        mode => root.rateText(mode)) : []
                                selectedIndex: root.selectedRefreshIndex(
                                    root.selectedOutput, root.currentResolutionGroup)
                                enabled: Boolean(root.selectedOutput
                                    && root.selectedOutput.modeSelectionSupported === true
                                    && root.currentResolutionGroup
                                    && root.currentResolutionGroup.rates.length > 0)
                                showChevron: enabled
                                onSelected: index => root.selectRefresh(index)
                            }
                        }

                        Form.SettingRow {
                            objectName: "displaySettingColorDepth"
                            label: "Color depth"
                            isLast: false

                            DisplaySelector {
                                objectName: "displayColorDepthControl"
                                controlWidth: 130
                                label: "Unavailable"
                                enabled: false
                                showChevron: false
                                textPrimary: Components.Theme.textTertiary
                                textSecondary: Components.Theme.textTertiary
                            }
                        }

                        Form.SettingRow {
                            objectName: "displaySettingVrr"
                            label: "Variable Refresh Rate"
                            sublabel: "VRR / FreeSync / G-Sync mode when supported by the display"
                            isLast: false

                            DisplaySelector {
                                objectName: "displayVrrControl"
                                controlWidth: 160
                                label: root.selectedOutput
                                    ? root.vrrText(root.selectedOutput.vrrState)
                                    : "Unavailable"
                                enabled: false
                                showChevron: false
                                textPrimary: Components.Theme.textTertiary
                                textSecondary: Components.Theme.textTertiary
                            }
                        }

                        Form.SettingRow {
                            objectName: "displaySettingScale"
                            label: "Display scale"
                            isLast: true

                            DisplaySelector {
                                objectName: "displayScaleControl"
                                controlWidth: 130
                                label: root.selectedOutput
                                    ? root.formatScale(
                                        root.selectedScaleMilli(root.selectedOutput))
                                    : "Unavailable"
                                options: root.scaleLabels(root.selectedOutput)
                                selectedIndex: root.selectedScaleIndex(root.selectedOutput)
                                enabled: Boolean(root.selectedOutput
                                    && root.selectedOutput.scaleMutationSupported === true
                                    && root.selectedMode(root.selectedOutput)
                                    && options.length > 0)
                                showChevron: enabled
                                onSelected: index => root.selectScale(index)
                            }
                        }
                    }
                }

                Form.SectionHeader {
                    objectName: "displayColorNightShiftHeader"
                    Layout.fillWidth: true
                    Layout.bottomMargin: 12
                    text: "COLOR & NIGHT SHIFT"
                }

                Form.FormCard {
                    objectName: "displayColorNightShiftCard"
                    Layout.bottomMargin: 24
                    radius: 12
                    margins: 0

                    ColumnLayout {
                        width: parent.width
                        spacing: 0

                        Form.SettingRow {
                            label: "Night Shift"
                            sublabel: "Warm screen colors to reduce blue light at night"
                            isLast: false
                            DisplaySelector {
                                objectName: "displayNightShiftControl"
                                controlWidth: 130
                                label: "Unavailable"
                                enabled: false
                                showChevron: false
                                textPrimary: Components.Theme.textTertiary
                                textSecondary: Components.Theme.textTertiary
                            }
                        }
                        Form.SettingRow {
                            label: "Night Shift strength"
                            sublabel: "Unavailable because the current backend has no color adjustment control"
                            isLast: false
                            DisplaySelector {
                                objectName: "displayNightShiftStrengthControl"
                                controlWidth: 130
                                label: "Unavailable"
                                enabled: false
                                showChevron: false
                                textPrimary: Components.Theme.textTertiary
                                textSecondary: Components.Theme.textTertiary
                            }
                        }
                        Form.SettingRow {
                            label: "Schedule"
                            sublabel: "Turn Night Shift on and off automatically"
                            isLast: false
                            DisplaySelector {
                                objectName: "displayNightShiftScheduleControl"
                                controlWidth: 130
                                label: "Unavailable"
                                enabled: false
                                showChevron: false
                                textPrimary: Components.Theme.textTertiary
                                textSecondary: Components.Theme.textTertiary
                            }
                        }
                        Form.SettingRow {
                            label: "Start"
                            sublabel: "When Night Shift should turn on"
                            isLast: false
                            DisplaySelector {
                                objectName: "displayNightShiftStartControl"
                                controlWidth: 130
                                label: "Unavailable"
                                enabled: false
                                showChevron: false
                                textPrimary: Components.Theme.textTertiary
                                textSecondary: Components.Theme.textTertiary
                            }
                        }
                        Form.SettingRow {
                            label: "End"
                            sublabel: "When Night Shift should turn off"
                            isLast: false
                            DisplaySelector {
                                objectName: "displayNightShiftEndControl"
                                controlWidth: 130
                                label: "Unavailable"
                                enabled: false
                                showChevron: false
                                textPrimary: Components.Theme.textTertiary
                                textSecondary: Components.Theme.textTertiary
                            }
                        }
                        Form.SettingRow {
                            label: "Saturation"
                            sublabel: "Unavailable because saturation adjustment is not supported"
                            isLast: true
                            DisplaySelector {
                                objectName: "displaySaturationControl"
                                controlWidth: 130
                                label: "Unavailable"
                                enabled: false
                                showChevron: false
                                textPrimary: Components.Theme.textTertiary
                                textSecondary: Components.Theme.textTertiary
                            }
                        }
                    }
                }

                RowLayout {
                    objectName: "displayApplyArea"
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    Layout.bottomMargin: 8
                    spacing: Components.Theme.spacingMedium

                    Item { Layout.fillWidth: true }

                    Controls.Button {
                        objectName: "displayApplyButton"
                        text: "Apply"
                        primary: true
                        enabled: Boolean(root.selectedOutput
                            && root.selectedOutput.canApplyDraft === true)
                        onClicked: {
                            if (root.selectedOutput && root.controller)
                                root.controller.applyDraft(root.selectedOutput.id)
                        }
                    }
                }
            }
        }
    }
}
