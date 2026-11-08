import QtQuick
import QtQuick.Window

Item {
    id: root

    property string source: ""
    property string fallbackText: ""
    property bool showFallbackText: true
    property size sourceSize: Qt.size(effectiveSourcePixelSize, effectiveSourcePixelSize)
    property bool retainWhileLoading: false
    property bool imageVisible: true
    readonly property int status: iconImage.status

    property int iconSize: 40
    property real maximumPresentationScale: 1.0
    property real maximumPresentationLogicalSize: 0
    property real devicePixelRatioOverride: 0
    property int iconRadius: 7
    property int iconPadding: 0
    property int fallbackRadius: 7
    property color fallbackColor: "#22FFFFFF"
    property color fallbackBorderColor: "transparent"
    property color fallbackTextColor: "#E8FFFFFF"
    property int fallbackBorderWidth: 0
    property int fallbackFontSize: 20
    property string appearanceOverride: ""
    property color tintColorOverride: "#0a84ff"
    property bool hasTintColorOverride: false
    property bool ready: iconImage.status === Image.Ready
    // Image retains its texture during a reload, but its status is Loading.
    // Keep both the direct image and the rounded shader visible in that state.
    property bool hasLoadedImage: false
    readonly property bool hasDisplayableImage: iconImage.status === Image.Ready
        || (root.retainWhileLoading && root.hasLoadedImage
            && iconImage.status === Image.Loading)
    // In-memory sources (window captures) can load synchronously so a source
    // change swaps in place instead of passing through an empty frame.
    property bool asynchronous: true

    readonly property bool hasThemeController:
        typeof ThemeController !== "undefined" && ThemeController !== null

    readonly property var knownAppearances: ["default", "dark", "clear", "tinted", "monochrome"]

    readonly property string effectiveAppearance: {
        var override = String(root.appearanceOverride).trim().toLowerCase()
        if (override.length > 0)
            return root.knownAppearances.indexOf(override) >= 0 ? override : "default"
        if (root.hasThemeController) {
            var configured = String(ThemeController.iconAppearance).trim().toLowerCase()
            if (root.knownAppearances.indexOf(configured) >= 0)
                return configured
        }
        return "default"
    }

    readonly property bool hasIconProvider:
        typeof AstreaIconProvider !== "undefined" && AstreaIconProvider !== null

    // App icons take the macOS styles (Dark, Clear, Tinted) from the icon
    // provider, which takes each icon apart into tile and glyph. Other icons,
    // and sources the provider does not serve, fall back to the shader.
    readonly property bool styledByProvider: root.appTileVariant.length > 0
        && root.hasIconProvider && AstreaIconProvider.appStylesAvailable
        && (root.effectiveAppearance === "dark" || root.effectiveAppearance === "clear"
            || root.effectiveAppearance === "tinted")
        && !/^(qrc|image|https?|data):/.test(String(root.source))
        && String(root.source).indexOf(":/") !== 0

    readonly property color effectiveTintColor: {
        if (root.hasTintColorOverride)
            return root.tintColorOverride
        if (root.hasThemeController && ThemeController.accentHex)
            return ThemeController.accentHex
        return "#0a84ff"
    }

    // The software scene graph cannot run ShaderEffect, so the presentation
    // shader would hide the icon and draw nothing; show the plain image there.
    readonly property bool shadersAvailable:
        GraphicsInfo.api !== GraphicsInfo.Software

    readonly property real presentationMode: {
        if (root.styledByProvider || !root.shadersAvailable)
            return 0.0
        if (root.effectiveAppearance === "monochrome" || root.effectiveAppearance === "clear")
            return 1.0
        if (root.effectiveAppearance === "tinted")
            return 2.0
        return 0.0
    }

    function hexByte(v) {
        var h = Math.round(Math.max(0, Math.min(1, v)) * 255).toString(16)
        return h.length < 2 ? "0" + h : h
    }

    readonly property string styleQuery: root.styledByProvider
        ? "&style=" + root.effectiveAppearance
            + (root.effectiveAppearance === "tinted"
               ? "&tint=" + hexByte(root.effectiveTintColor.r) + hexByte(root.effectiveTintColor.g)
                   + hexByte(root.effectiveTintColor.b)
               : "")
        : ""

    readonly property real effectiveDevicePixelRatio: {
        var value = devicePixelRatioOverride > 0
            ? devicePixelRatioOverride : Screen.devicePixelRatio
        if (!isFinite(value) || value <= 0)
            value = 1
        value = Math.max(0.25, Math.min(4.0, value))
        return Math.round(value * 1000) / 1000
    }

    readonly property int effectiveMaximumLogicalSize: {
        var value = maximumPresentationLogicalSize > 0
            ? maximumPresentationLogicalSize : iconSize * maximumPresentationScale
        if (!isFinite(value) || value <= 0)
            value = 1
        return Math.max(1, Math.min(256, Math.ceil(value)))
    }

    readonly property int effectiveSourcePixelSize: Math.max(
        1, Math.min(1024, Math.ceil(effectiveMaximumLogicalSize
                                    * effectiveDevicePixelRatio)))

    // "light" or "dark": app art the active Astrea icon theme lacks is wrapped
    // in that theme's macOS tile. Empty presents art as it is.
    property string appTileVariant: ""

    property int retryCount: 0
    property int retryNonce: 0
    property int maxRetries: 2
    property int retryDelay: 180

    Timer {
        id: retryTimer
        interval: root.retryDelay
        onTriggered: {
            root.retryCount++
            root.retryNonce++
        }
    }

    function retry() {
        if (retryCount < maxRetries)
            retryTimer.restart()
    }

    readonly property string resolvedSource: {
        var name = root.source
        if (!name) return ""
        var revision = (typeof AstreaIconProvider !== "undefined" && AstreaIconProvider)
            ? AstreaIconProvider.themeRevision : 0
        // Local files take the provider only while tiles are on, so they can
        // be wrapped; otherwise they keep the direct file path.
        var tiled = root.appTileVariant.length > 0
            && (name.startsWith("/") || name.startsWith("file://"))
            && root.hasIconProvider
            && (AstreaIconProvider.appTilesActive || root.styledByProvider)
        if (!tiled) {
            if (/^[a-zA-Z][a-zA-Z0-9+.-]*:/.test(name)) return name
            if (name.indexOf("/") >= 0)
                return name.startsWith("file://") ? name : Qt.resolvedUrl(name)
        }
        return "image://astrea-icon/" + encodeURIComponent(name)
            + "?logicalSize=" + root.effectiveMaximumLogicalSize
            + "&dpr=" + root.effectiveDevicePixelRatio.toFixed(3)
            + "&pixelSize=" + root.effectiveSourcePixelSize
            + (root.appTileVariant.length > 0 ? "&tile=" + root.appTileVariant : "")
            + root.styleQuery
            + "&revision=" + revision + "&retry=" + root.retryNonce
    }

    onSourceChanged: retryCount = 0

    Image {
        id: iconImage
        anchors.fill: parent
        anchors.margins: root.iconPadding
        source: root.resolvedSource
        sourceSize: root.sourceSize
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        asynchronous: root.asynchronous
        cache: true
        retainWhileLoading: root.retainWhileLoading
        // Keep this Image as the shader's direct texture source. It is hidden
        // only while the shader owns the draw, so rounded mode cannot render
        // the source twice.
        visible: root.imageVisible && root.hasDisplayableImage && root.iconRadius <= 0
            && root.presentationMode === 0

        onStatusChanged: {
            if (status === Image.Ready)
                root.hasLoadedImage = true
            else if (status !== Image.Loading || !root.retainWhileLoading)
                root.hasLoadedImage = false
            if (status === Image.Error && root.retryCount < root.maxRetries)
                root.retry()
        }
    }

    Loader {
        id: roundedEffectLoader
        objectName: "roundedIconEffectLoader"
        anchors.fill: iconImage
        active: root.imageVisible && root.hasDisplayableImage
            && (root.iconRadius > 0 || root.presentationMode !== 0)
        sourceComponent: ShaderEffect {
            objectName: "roundedIconEffect"
            anchors.fill: parent
            property variant source: iconImage
            property real roundedRadius: root.iconRadius
                / Math.min(width, height)
            property real presentationMode: root.presentationMode
            property color tintColor: root.effectiveTintColor
            // Keep qt_TexCoord0 in the local [0, 1] range. Qt detaches an
            // atlas texture when necessary, which preserves correct sampling
            // for this non-linear UV-space mask.
            supportsAtlasTextures: false
            blending: true
            fragmentShader: "qrc:/shaders/rounded_icon.frag.qsb"
        }
    }

    Rectangle {
        objectName: "fallbackSurface"
        anchors.fill: parent
        radius: root.fallbackRadius
        color: root.fallbackColor
        border.color: root.fallbackBorderColor
        border.width: root.fallbackBorderWidth
        visible: root.imageVisible && !root.hasDisplayableImage && root.showFallbackText

        Text {
            anchors.centerIn: parent
            text: root.fallbackText
            font.pixelSize: root.fallbackFontSize > 0
                ? root.fallbackFontSize
                : Math.max(11, Math.round(Math.min(parent.width, parent.height) * 0.36))
            font.weight: Font.Medium
            color: root.fallbackTextColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
