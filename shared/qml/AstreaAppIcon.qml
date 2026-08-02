import QtQuick
import Qt5Compat.GraphicalEffects

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
    property int sourcePixelSize: 80
    property int iconRadius: 7
    property int iconPadding: 0
    property int fallbackRadius: 7
    property color fallbackColor: "#22FFFFFF"
    property color fallbackBorderColor: "transparent"
    property color fallbackTextColor: "#E8FFFFFF"
    property int fallbackBorderWidth: 0
    property int fallbackFontSize: 20
    property bool ready: iconImage.status === Image.Ready

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
            + "?size=" + root.sourcePixelSize
            + "&revision=" + revision + "&retry=" + root.retryNonce
    }

    onBestIconNameChanged: retryCount = 0

    Image {
        id: iconImage
        anchors.fill: parent
        anchors.margins: root.iconPadding
        source: root.resolvedSource
        sourceSize.width: root.sourcePixelSize
        sourceSize.height: root.sourcePixelSize
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        asynchronous: true
        cache: true
        visible: status === Image.Ready && root.iconRadius <= 0

        onStatusChanged: {
            if (status === Image.Error && root.retryCount < root.maxRetries)
                root.retry()
        }
    }

    OpacityMask {
        anchors.fill: iconImage
        source: iconImage
        maskSource: Rectangle {
            width: iconImage.width
            height: iconImage.height
            radius: root.iconRadius > 0 ? root.iconRadius : 0
            visible: false
        }
        visible: iconImage.status === Image.Ready && root.iconRadius > 0
    }

    Rectangle {
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
