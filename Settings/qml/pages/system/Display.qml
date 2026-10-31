import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Window
import "DisplayLogic.js" as DisplayLogic
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
        Boolean(root.controller && DisplayLogic.nonEmpty(root.controller.lastError))
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
        DisplayLogic.resolutionGroups(root.selectedOutput)
    readonly property var defaultResolutionGroups:
        DisplayLogic.defaultResolutionSubset(root.allResolutionGroups)
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

    function nonEmpty(value) { return DisplayLogic.nonEmpty(value) }

    function isBuiltIn(output) { return DisplayLogic.isBuiltIn(output) }

    function titleFor(output) { return DisplayLogic.titleFor(output) }

    function kindFor(output) { return DisplayLogic.kindFor(output) }

    function subtitleFor(output) { return DisplayLogic.subtitleFor(output) }

    function monitorSelectorLabel(output) { return DisplayLogic.monitorSelectorLabel(output) }

    function isQuarterTurn(output) { return DisplayLogic.isQuarterTurn(output) }

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

    function resolutionKey(group) { return DisplayLogic.resolutionKey(group) }

    function formatResolution(group) { return DisplayLogic.formatResolution(group) }

    function formatRate(milliHz) { return DisplayLogic.formatRate(milliHz) }

    function formatScale(scaleMilli) { return DisplayLogic.formatScale(scaleMilli) }

    function currentMode(output) { return DisplayLogic.currentMode(output) }

    function modeById(output, modeId) { return DisplayLogic.modeById(output, modeId) }

    function selectedMode(output) { return DisplayLogic.selectedMode(output) }

    function selectedScaleMilli(output) { return DisplayLogic.selectedScaleMilli(output) }

    function selectedTransform(output) { return DisplayLogic.selectedTransform(output) }

    function rateText(mode) { return DisplayLogic.rateText(mode) }

    function refreshLabel(output) { return DisplayLogic.refreshLabel(output) }

    function selectedRefreshLabel(output) { return DisplayLogic.selectedRefreshLabel(output) }

    function modeSummary(output) { return DisplayLogic.modeSummary(output) }

    function vrrText(state) { return DisplayLogic.vrrText(state) }

    function resolutionGroups(output) { return DisplayLogic.resolutionGroups(output) }

    function modeForResolution(group, output) { return DisplayLogic.modeForResolution(group, output) }

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
        const mode = DisplayLogic.modeForResolution(group, output)
        if (!mode)
            return
        root.setDraftForMode(output, mode, DisplayLogic.selectedScaleMilli(output),
            DisplayLogic.selectedTransform(output))
    }

    function selectedRefreshIndex(output, group) { return DisplayLogic.selectedRefreshIndex(output, group) }

    function selectRefresh(index) {
        const output = root.selectedOutput
        const group = root.currentResolutionGroup
        if (!output || output.modeSelectionSupported !== true || !group
                || index < 0 || index >= group.rates.length)
            return
        root.setDraftForMode(output, group.rates[index],
            DisplayLogic.selectedScaleMilli(output), DisplayLogic.selectedTransform(output))
    }

    function colorDepthText(bits) { return DisplayLogic.colorDepthText(bits) }

    function colorDepthOptions(output) { return DisplayLogic.colorDepthOptions(output) }

    function selectedColorDepthIndex(output) { return DisplayLogic.selectedColorDepthIndex(output) }

    function selectColorDepth(index) {
        const output = root.selectedOutput
        const options = DisplayLogic.colorDepthOptions(output)
        if (!root.controller || !output
                || output.colorDepthSelectionSupported !== true
                || index < 0 || index >= options.length)
            return
        root.controller.setDraftColorDepth(String(output.id), options[index])
    }

    function scaleValues(output) { return DisplayLogic.scaleValues(output) }

    function scaleLabels(output) { return DisplayLogic.scaleLabels(output) }

    function selectedScaleIndex(output) { return DisplayLogic.selectedScaleIndex(output) }

    function selectScale(index) {
        const output = root.selectedOutput
        const values = DisplayLogic.scaleValues(output)
        if (!output || output.scaleMutationSupported !== true
                || index < 0 || index >= values.length)
            return
        const mode = DisplayLogic.selectedMode(output)
        if (!mode)
            return
        root.setDraftForMode(output, mode, values[index],
            DisplayLogic.selectedTransform(output))
    }

    function defaultResolutionSubset(groups) { return DisplayLogic.defaultResolutionSubset(groups) }

    Component.onCompleted: {
        root.reconcileSelection()
        if (root.controller)
            root.controller.refresh()
        if (root.wallpaper && !root.wallpaper.busy)
            root.wallpaper.refresh()
    }

    Connections {
        target: root.controller
        function onSnapshotChanged() {
            root.reconcileSelection()
            root.transactionDeadlineMs = root.transactionActive
                ? Date.now() + Number(root.controller.transactionRemainingMs) : 0
        }
    }

    // A temporary apply waits for Keep or Revert and Typhon reverts it on
    // timeout. The controller only learns about that from a new snapshot,
    // so poll while a transaction is open and count the deadline down here.
    readonly property bool transactionActive:
        Boolean(root.controller && root.controller.transactionActive)
    readonly property bool confirmationPending:
        Boolean(root.controller && root.controller.confirmationPending)
    readonly property string transactionState:
        root.controller ? String(root.controller.transactionState) : "none"
    property double transactionDeadlineMs: 0
    property int transactionSecondsLeft: 0
    readonly property string transactionStatusText: {
        if (root.transactionState === "pending_confirmation")
            return "Keep these display settings? Reverting in "
                + root.transactionSecondsLeft + " s."
        if (root.transactionState === "persistence_pending")
            return "Saving display settings…"
        if (root.transactionState === "rolling_back")
            return "Reverting display settings…"
        if (root.transactionState === "rollback_failed")
            return "Could not revert the display settings."
        return ""
    }

    function updateTransactionCountdown() {
        root.transactionSecondsLeft = Math.max(0,
            Math.ceil((root.transactionDeadlineMs - Date.now()) / 1000))
    }

    Timer {
        objectName: "displayTransactionPoll"
        interval: 1000
        repeat: true
        running: root.transactionActive
        triggeredOnStart: true
        onTriggered: {
            root.updateTransactionCountdown()
            if (root.controller && !root.controller.busy)
                root.controller.refresh()
        }
    }

    // Rectangle with continuous corners; strokes are drawn inside the bounds.
    component ContinuousRect: Shape {
        id: continuousRect
        property real radius: 0
        property color color: "transparent"
        property color borderColor: "transparent"
        property real borderWidth: 0

        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: continuousRect.color
            strokeColor: continuousRect.borderWidth > 0
                ? continuousRect.borderColor : "transparent"
            strokeWidth: continuousRect.borderWidth > 0 ? continuousRect.borderWidth : -1
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: DisplayLogic.continuousRectPath(
                    continuousRect.width, continuousRect.height,
                    continuousRect.radius, continuousRect.borderWidth / 2)
            }
        }
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

        readonly property var layout:
            DisplayLogic.previewLayout(root.outputs, width, height, 28, 20, preview.nameStrip)
        property bool previewReady: false
        // Room under every device for its name, as macOS labels them.
        readonly property real nameStrip: 22

        Component.onCompleted: preview.previewReady = true

        Repeater {
            model: root.outputs.length

            delegate: Item {
                id: monitorItem
                required property int index
                readonly property var modelData: root.outputs[index]
                objectName: "displayPreviewMonitor_" + String(modelData.id)

                readonly property bool active:
                    String(modelData.id) === root.selectedOutputId
                readonly property bool laptop: DisplayLogic.isBuiltIn(modelData)
                readonly property bool hasCurrentMode:
                    Boolean(modelData.hasCurrentMode)
                readonly property var slot: preview.layout[index]
                    || { x: 0, y: 0, screenWidth: 0, screenHeight: 0, bezel: 10, below: 26 }
                readonly property real screenWidth: slot.screenWidth
                readonly property real screenHeight: slot.screenHeight
                readonly property real bezel: slot.bezel
                // Concentric corners: every inner radius is the outer radius
                // minus the distance between the two edges.
                // Tight radii like Apple hardware: the glass corners stay
                // nearly square while the body only softens them.
                readonly property real cornerRadius: laptop ? 11 : 10
                readonly property real screenInset: laptop ? 4 : 5
                // The glass stays black in both themes; only the stand follows the theme.
                readonly property bool lightDevice: Components.Theme.isLight
                // White stand on dark pages; light silver on light pages so it
                // still reads against the background. Opaque so the shadows
                // never show through it.
                readonly property color standTint: lightDevice ? "#d4d4da" : "#e6e6eb"

                width: screenWidth + bezel * 2
                height: bezel + screenHeight + slot.below
                x: slot.x
                y: slot.y

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

                // Layering, back to front: shadows, stand, body.
                // The body always sits on top so the stand emerges from behind it.

                // Ambient elevation, the way macOS lifts a window off the desktop.
                RectangularShadow {
                    z: -3
                    anchors.fill: screenFrame
                    radius: screenFrame.radius
                    offset.y: 12
                    blur: 36
                    spread: -6
                    color: Qt.rgba(0, 0, 0, monitorItem.lightDevice ? 0.18 : 0.38)
                }

                RectangularShadow {
                    z: -3
                    anchors.fill: screenFrame
                    radius: screenFrame.radius
                    offset.y: 1
                    blur: 3
                    color: Qt.rgba(0, 0, 0, monitorItem.lightDevice ? 0.14 : 0.30)
                }

                Rectangle {
                    id: screenFrame
                    x: monitorItem.bezel
                    y: monitorItem.bezel
                    width: monitorItem.screenWidth
                    height: monitorItem.screenHeight
                    radius: monitorItem.cornerRadius
                    color: "transparent"

                    Behavior on width {
                        enabled: preview.previewReady
                        NumberAnimation { duration: Components.Theme.animationNormal }
                    }
                    Behavior on height {
                        enabled: preview.previewReady
                        NumberAnimation { duration: Components.Theme.animationNormal }
                    }

                    // Device body with a hairline rim.
                    ContinuousRect {
                        objectName: "displayMonitorBody_" + String(modelData.id)
                        anchors.fill: parent
                        radius: parent.radius
                        color: "#0a0a0c"
                        borderWidth: 1
                        borderColor: Qt.rgba(1, 1, 1, 0.16)
                    }

                    Item {
                        id: screenMask
                        anchors.fill: screenSurface
                        visible: false
                        layer.enabled: true
                        layer.samples: 4

                        ContinuousRect {
                            anchors.fill: parent
                            radius: screenSurface.radius
                            color: "white"
                        }
                    }

                    Rectangle {
                        id: screenSurface
                        anchors.fill: parent
                        anchors.margins: monitorItem.screenInset
                        radius: Math.max(2, screenFrame.radius - monitorItem.screenInset)
                        color: "#000000"
                        layer.enabled: true
                        layer.smooth: true
                        layer.effect: MultiEffect {
                            maskEnabled: true
                            maskSource: screenMask
                            maskThresholdMin: 0.5
                            maskSpreadAtMin: 1.0
                        }

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
                                    && DisplayLogic.nonEmpty(root.wallpaper.effectivePreviewUrl)
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
                            cache: true
                            retainWhileLoading: true
                            smooth: true
                            mipmap: true
                            // A fixed decode bound avoids reloading at every animation frame.
                            sourceSize.width: 1024
                            sourceSize.height: 1024
                        }

                        // Glass: a soft sheen across the top and a recessed panel edge.
                        Rectangle {
                            anchors.fill: parent
                            gradient: Gradient {
                                GradientStop { position: 0; color: Qt.rgba(1, 1, 1, 0.05) }
                                GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0) }
                            }
                        }

                        ContinuousRect {
                            anchors.fill: parent
                            radius: screenSurface.radius
                            borderWidth: 1
                            borderColor: Qt.rgba(0, 0, 0, 0.30)
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
                    }

                    // MacBook camera housing, cut into the top of the glass.
                    Rectangle {
                        objectName: "displayLaptopNotch_" + String(modelData.id)
                        visible: monitorItem.laptop
                        anchors.horizontalCenter: screenSurface.horizontalCenter
                        anchors.top: screenSurface.top
                        width: Math.max(14, screenSurface.width * 0.13)
                        height: Math.max(3, Math.round(screenSurface.height * 0.035))
                        bottomLeftRadius: height * 0.6
                        bottomRightRadius: height * 0.6
                        color: "#0a0a0c"
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.selectedOutputId = String(modelData.id)
                    }
                }

                // Studio Display stand: a broad neck rising from behind the
                // body onto a thin flat foot. The neck sits a shade darker,
                // in the body's shadow, so the two read as separate planes.
                Rectangle {
                    objectName: "displayMonitorStand_" + String(modelData.id)
                    z: -1
                    visible: !monitorItem.laptop
                    anchors.horizontalCenter: screenFrame.horizontalCenter
                    y: screenFrame.y + screenFrame.height - 8
                    width: Math.max(18, screenFrame.width * 0.15)
                    height: 30
                    color: Qt.darker(monitorItem.standTint, 1.10)

                    // Shadow the body casts onto the neck just below its edge.
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        y: 8
                        height: 10
                        gradient: Gradient {
                            GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.32) }
                            GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0) }
                        }
                    }
                }

                RectangularShadow {
                    z: -2
                    visible: !monitorItem.laptop
                    anchors.fill: standBase
                    radius: standBase.radius
                    offset.y: 2
                    blur: 8
                    spread: 1
                    color: Qt.rgba(0, 0, 0, monitorItem.lightDevice ? 0.22 : 0.55)
                }

                Rectangle {
                    id: standBase
                    objectName: "displayMonitorStandBase_" + String(modelData.id)
                    z: -1
                    visible: !monitorItem.laptop
                    anchors.horizontalCenter: screenFrame.horizontalCenter
                    y: screenFrame.y + screenFrame.height + 21
                    width: Math.max(52, screenFrame.width * 0.34)
                    height: 4
                    radius: 2
                    color: monitorItem.standTint
                }

                RectangularShadow {
                    z: -2
                    visible: monitorItem.laptop
                    anchors.fill: laptopDeck
                    radius: laptopDeck.bottomLeftRadius
                    offset.y: 3
                    blur: 10
                    spread: 1
                    color: Qt.rgba(0, 0, 0, monitorItem.lightDevice ? 0.22 : 0.55)
                }

                // MacBook base seen from the front: a slim deck a little wider
                // than the lid, a darker front lip, and the thumb indent.
                Rectangle {
                    id: laptopDeck
                    objectName: "displayLaptopChassis_" + String(modelData.id)
                    z: -1
                    visible: monitorItem.laptop
                    anchors.horizontalCenter: screenFrame.horizontalCenter
                    y: screenFrame.y + screenFrame.height
                    width: screenFrame.width + Math.max(16, screenFrame.width * 0.08)
                    height: 8
                    topLeftRadius: 1
                    topRightRadius: 1
                    bottomLeftRadius: 5
                    bottomRightRadius: 5
                    color: monitorItem.standTint

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 3
                        bottomLeftRadius: parent.bottomLeftRadius
                        bottomRightRadius: parent.bottomRightRadius
                        color: Qt.darker(monitorItem.standTint, 1.12)
                    }

                    // The lid's shadow falls across the top of the deck.
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: screenFrame.width
                        height: 3
                        gradient: Gradient {
                            GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.30) }
                            GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0) }
                        }
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.max(22, parent.width * 0.12)
                        height: 3
                        bottomLeftRadius: 3
                        bottomRightRadius: 3
                        color: Qt.darker(monitorItem.standTint, 1.18)
                    }
                }

                // Name under the device; the selected one reads at full strength.
                Text {
                    id: connectorLabel
                    objectName: "displayConnector"
                    anchors.horizontalCenter: screenFrame.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: Math.max(screenFrame.width + 24, 72)
                    height: preview.nameStrip - 6
                    verticalAlignment: Text.AlignBottom
                    horizontalAlignment: Text.AlignHCenter
                    text: DisplayLogic.nonEmpty(modelData.name)
                        ? String(modelData.name) : "Display"
                    textFormat: Text.PlainText
                    color: monitorItem.active || root.outputs.length < 2
                        ? Components.Theme.textPrimary : Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    font.weight: monitorItem.active
                        ? Components.Theme.fontWeightMedium : Font.Normal
                    elide: Text.ElideRight

                    Behavior on color {
                        ColorAnimation { duration: Components.Theme.animationFast }
                    }
                }
            }
        }
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        ColumnLayout {
            id: pageContents
            objectName: "displayPageContent"
            Layout.fillWidth: true
            width: parent.width
            spacing: 0

            Form.PageHeader {
                title: I18n.tr("settings.nav.display", "Display")
                subtitle: I18n.tr("settings.pages.display.subtitle", "Resolution, refresh rate, scale and color.")
            }

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
                                    text: DisplayLogic.titleFor(root.selectedOutput)
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
                                    text: DisplayLogic.subtitleFor(root.selectedOutput)
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
                                    text: DisplayLogic.modeSummary(root.selectedOutput)
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
                        displayText: DisplayLogic.monitorSelectorLabel(root.selectedOutput)
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
                            text: DisplayLogic.monitorSelectorLabel(root.selectedOutput)
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
                                text: DisplayLogic.monitorSelectorLabel(modelData)
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

                Form.FormCard {
                    objectName: "displayResolutionList"
                    Layout.fillWidth: true
                    Layout.bottomMargin: 24
                    margins: 4
                    spacing: 0

                    Repeater {
                        model: root.visibleResolutionGroups

                        delegate: Rectangle {
                            required property var modelData
                            readonly property bool selected: Boolean(modelData.current)
                            readonly property bool interactive:
                                Boolean(root.selectedOutput)
                                    && root.selectedOutput.modeSelectionSupported === true
                                    && modelData.rates.length > 0
                            readonly property string key: DisplayLogic.resolutionKey(modelData)

                            objectName: "displayResolutionRow_" + key
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            implicitHeight: 36
                            radius: Components.Theme.controlRadius
                            color: selected
                                ? Qt.rgba(Components.Theme.accent.r,
                                          Components.Theme.accent.g,
                                          Components.Theme.accent.b, 0.12)
                                : (interactive && resolutionActivation.containsMouse
                                    ? Qt.rgba(Components.Theme.accent.r,
                                              Components.Theme.accent.g,
                                              Components.Theme.accent.b, 0.06)
                                    : "transparent")
                            border.width: interactive && activeFocus ? 1 : 0
                            border.color: interactive && activeFocus
                                ? Components.Theme.accent : "transparent"
                            activeFocusOnTab: interactive
                            Accessible.role: Accessible.Button
                            Accessible.name: DisplayLogic.formatResolution(modelData)

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
                                    text: DisplayLogic.formatResolution(modelData)
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
                        Layout.leftMargin: 12
                        Layout.bottomMargin: 4
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
                        Layout.leftMargin: 12
                        Layout.bottomMargin: 4
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
                                label: DisplayLogic.selectedRefreshLabel(root.selectedOutput)
                                options: root.currentResolutionGroup
                                    ? root.currentResolutionGroup.rates.map(
                                        mode => DisplayLogic.rateText(mode)) : []
                                selectedIndex: DisplayLogic.selectedRefreshIndex(
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
                                label: root.selectedOutput
                                    ? DisplayLogic.colorDepthText(root.selectedOutput.colorDepthBits)
                                    : "Unavailable"
                                options: DisplayLogic.colorDepthOptions(root.selectedOutput).map(
                                    bits => DisplayLogic.colorDepthText(bits))
                                selectedIndex: DisplayLogic.selectedColorDepthIndex(root.selectedOutput)
                                enabled: Boolean(root.selectedOutput
                                    && root.selectedOutput.colorDepthSelectionSupported === true)
                                showChevron: enabled
                                textPrimary: enabled
                                    ? Components.Theme.textPrimary : Components.Theme.textTertiary
                                textSecondary: enabled
                                    ? Components.Theme.textSecondary : Components.Theme.textTertiary
                                onSelected: index => root.selectColorDepth(index)
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
                                    ? DisplayLogic.vrrText(root.selectedOutput.vrrState)
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
                                    ? DisplayLogic.formatScale(
                                        DisplayLogic.selectedScaleMilli(root.selectedOutput))
                                    : "Unavailable"
                                options: DisplayLogic.scaleLabels(root.selectedOutput)
                                selectedIndex: DisplayLogic.selectedScaleIndex(root.selectedOutput)
                                enabled: Boolean(root.selectedOutput
                                    && root.selectedOutput.scaleMutationSupported === true
                                    && DisplayLogic.selectedMode(root.selectedOutput)
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

                    Text {
                        objectName: "displayTransactionStatus"
                        visible: root.transactionActive
                        Layout.fillWidth: true
                        text: root.transactionStatusText
                        textFormat: Text.PlainText
                        color: root.transactionState === "rollback_failed"
                            ? Components.Theme.errorColor : Components.Theme.textPrimary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeNormal
                        wrapMode: Text.WordWrap
                    }

                    Item {
                        visible: !root.transactionActive
                        Layout.fillWidth: true
                    }

                    Controls.Button {
                        objectName: "displayRevertButton"
                        visible: root.confirmationPending
                        text: "Revert"
                        enabled: Boolean(root.controller && !root.controller.busy)
                        onClicked: {
                            if (root.controller)
                                root.controller.revert()
                        }
                    }

                    Controls.Button {
                        objectName: "displayKeepButton"
                        visible: root.confirmationPending
                        text: "Keep changes"
                        primary: true
                        enabled: Boolean(root.controller && !root.controller.busy)
                        onClicked: {
                            if (root.controller)
                                root.controller.confirm()
                        }
                    }

                    Controls.Button {
                        objectName: "displayApplyButton"
                        visible: !root.confirmationPending
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
