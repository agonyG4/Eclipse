import QtQuick
import QtQuick.Window

Item {
    id: root

    property string iconSource: ""
    property string iconUrl: ""
    property string astreaIcon: ""
    property string astreaIconName: ""
    property string iconName: ""
    property string iconPath: ""
    property string icon: ""
    property string className: ""
    property string title: ""
    property string fallbackIconName: ""
    property string appName: ""
    property bool iconPending: false
    property bool showFallbackText: true

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
    property bool ready: iconImage.status === Image.Ready

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

    readonly property string effectiveIconName: {
        if (iconSource) return iconSource
        if (iconUrl) return iconUrl
        if (astreaIcon) return astreaIcon
        if (astreaIconName) return astreaIconName
        if (iconName) return iconName
        if (iconPath) return iconPath
        if (icon) return icon
        if (className) return className
        if (title && title !== appName) return title
        return fallbackIconName
    }

    readonly property string bestIconName: effectiveIconName
    readonly property string initials: {
        if (!appName) return ""
        var cleaned = appName.replace(/[-_.]/g, " ")
        var parts = cleaned.trim().split(/\s+/).filter(function(part) { return part.length > 0 })
        if (parts.length === 0) return ""
        if (parts.length === 1) return parts[0][0].toUpperCase()
        return (parts[0][0] + parts[1][0]).toUpperCase()
    }

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
        var name = root.bestIconName
        if (!name) return ""
        var revision = (typeof AstreaIconProvider !== "undefined" && AstreaIconProvider)
            ? AstreaIconProvider.themeRevision : 0
        if (name.indexOf("://") >= 0) return name
        if (name.indexOf("/") >= 0)
            return name.startsWith("file://") ? name : Qt.resolvedUrl(name)
        return "image://astrea-icon/" + encodeURIComponent(name)
            + "?logicalSize=" + root.effectiveMaximumLogicalSize
            + "&dpr=" + root.effectiveDevicePixelRatio.toFixed(3)
            + "&pixelSize=" + root.effectiveSourcePixelSize
            + "&revision=" + revision + "&retry=" + root.retryNonce
    }

    onBestIconNameChanged: retryCount = 0

    Image {
        id: iconImage
        anchors.fill: parent
        anchors.margins: root.iconPadding
        source: root.resolvedSource
        sourceSize.width: root.effectiveSourcePixelSize
        sourceSize.height: root.effectiveSourcePixelSize
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        asynchronous: true
        cache: true
        // Keep this Image as the shader's direct texture source. It is hidden
        // only while the shader owns the draw, so rounded mode cannot render
        // the source twice.
        visible: status === Image.Ready && root.iconRadius <= 0

        onStatusChanged: {
            if (status === Image.Error && root.retryCount < root.maxRetries)
                root.retry()
        }
    }

    Loader {
        id: roundedEffectLoader
        objectName: "roundedIconEffectLoader"
        anchors.fill: iconImage
        active: iconImage.status === Image.Ready && root.iconRadius > 0
        sourceComponent: ShaderEffect {
            objectName: "roundedIconEffect"
            anchors.fill: parent
            property variant source: iconImage
            property real roundedRadius: root.iconRadius
                / Math.min(width, height)
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
        visible: iconImage.status !== Image.Ready && root.showFallbackText

        Text {
            anchors.centerIn: parent
            text: root.initials
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
