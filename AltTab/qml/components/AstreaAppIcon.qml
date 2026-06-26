import QtQuick
import Qt5Compat.GraphicalEffects

Item {
    id: root

    property string iconSource: ""
    property string iconName: ""
    property string iconPath: ""
    property bool iconPending: false
    property bool showFallbackText: true
    property string appName: ""

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

    property bool ready: icon.status === Image.Ready

    readonly property string effectiveIconSource: {
        if (iconSource) return iconSource
        if (iconPath) return Qt.resolvedUrl(iconPath)
        if (iconName) return "image://astrea-icon/" + encodeURIComponent(iconName) + "?size=" + sourcePixelSize + "&revision=" + (typeof AstreaIconProvider !== "undefined" && AstreaIconProvider ? AstreaIconProvider.themeRevision : 0) + "&retry=" + retryNonce
        return ""
    }

    readonly property string initials: {
        if (!appName) return ""
        var cleaned = appName.replace(/[-_.]/g, " ")
        var parts = cleaned.trim().split(/\s+/).filter(function(p) { return p.length > 0 })
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

    onIconNameChanged: retryCount = 0
    onIconSourceChanged: retryCount = 0
    onIconPathChanged: retryCount = 0

    Image {
        id: icon
        anchors.fill: parent
        anchors.margins: root.iconPadding
        source: root.effectiveIconSource
        sourceSize.width: root.sourcePixelSize
        sourceSize.height: root.sourcePixelSize
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        asynchronous: true
        cache: true
        visible: status === Image.Ready && root.iconRadius <= 0

        onStatusChanged: {
            if (status === Image.Error) {
                if (root.retryCount < root.maxRetries)
                    root.retry()
            }
        }
    }

    OpacityMask {
        anchors.fill: icon
        source: icon
        maskSource: Rectangle {
            width: icon.width
            height: icon.height
            radius: root.iconRadius > 0 ? root.iconRadius : 0
            visible: false
        }
        visible: icon.status === Image.Ready && root.iconRadius > 0
        invert: false
    }

    Rectangle {
        id: fallback
        anchors.fill: parent
        radius: root.fallbackRadius
        color: root.fallbackColor
        border.color: root.fallbackBorderColor
        border.width: root.fallbackBorderWidth
        visible: icon.status !== Image.Ready && root.showFallbackText

        Text {
            anchors.centerIn: parent
            text: root.initials
            font.pixelSize: Math.max(11, Math.round(Math.min(parent.width, parent.height) * 0.36))
            font.weight: Font.Medium
            color: root.fallbackTextColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
