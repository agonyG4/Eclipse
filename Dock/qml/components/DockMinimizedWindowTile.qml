import QtQuick
import QtQuick.Effects
import Astrea.Shared as Shared

// The visual of a minimized window in the Dock, after macOS: the window
// itself scaled into the icon square with its own rounded corners, a hairline
// edge and a soft contact shadow, badged with its app icon.
Item {
    id: root

    property url source
    property string iconName: ""
    property string iconPath: ""
    property string appName: ""
    property real maximumPresentationScale: 1
    property bool pressed: false
    // The magnification scale; detail sizes stay crisp relative to it.
    property real presentationScale: 1
    readonly property bool imageReady: windowImage.status === Image.Ready
    readonly property real paintedWidth: windowImage.paintedWidth
    readonly property real paintedHeight: windowImage.paintedHeight
    readonly property real cornerRadius: Math.max(2, Math.min(width, height) * 0.075)

    Image {
        id: windowImage
        objectName: root.objectName.length > 0 ? root.objectName + "-image" : ""
        anchors.fill: parent
        anchors.margins: 1
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        asynchronous: true
        // Every capture gets a new revision URL, so caching by URL only keeps
        // snapshot-driven delegate rebuilds from flashing.
        cache: true
        source: root.source
        visible: false
    }

    Item {
        id: windowMask
        anchors.fill: windowImage
        visible: false
        layer.enabled: true
        layer.smooth: true
        Rectangle {
            anchors.centerIn: parent
            width: windowImage.paintedWidth
            height: windowImage.paintedHeight
            radius: root.cornerRadius
            color: "white"
            antialiasing: true
        }
    }

    Item {
        id: windowLayer
        anchors.fill: windowImage
        visible: root.imageReady
        opacity: root.pressed ? 0.72 : 1
        Behavior on opacity { NumberAnimation { duration: 90 } }
        layer.enabled: visible
        layer.smooth: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#000000"
            shadowOpacity: 0.5
            shadowBlur: 0.45
            shadowVerticalOffset: 1.5
            autoPaddingEnabled: true
        }

        MultiEffect {
            anchors.fill: parent
            source: windowImage
            maskEnabled: true
            maskSource: windowMask
            maskThresholdMin: 0.4
            maskSpreadAtMin: 0.6
        }

        // A light hairline keeps dark and translucent windows legible on the
        // dark Dock material, like the outline macOS draws around windows.
        Rectangle {
            anchors.centerIn: parent
            width: windowImage.paintedWidth
            height: windowImage.paintedHeight
            radius: root.cornerRadius
            color: "transparent"
            border.width: 1 / Math.max(1, root.presentationScale)
            border.color: Qt.rgba(1, 1, 1, 0.22)
            antialiasing: true
        }
    }

    Shared.AstreaAppIcon {
        anchors.centerIn: parent
        width: parent.width * 0.78
        height: width
        iconName: root.iconName
        iconPath: root.iconPath
        appName: root.appName
        maximumPresentationScale: root.maximumPresentationScale
        iconRadius: 9
        fallbackRadius: 9
        opacity: 0.85
        visible: !root.imageReady
    }

    Item {
        objectName: root.objectName.length > 0 ? root.objectName + "-badge" : ""
        width: Math.round(parent.width * 0.36)
        height: width
        x: parent.width - width * 0.92
        y: parent.height - height * 0.92
        visible: root.imageReady
        layer.enabled: visible
        layer.smooth: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#000000"
            shadowOpacity: 0.45
            shadowBlur: 0.35
            shadowVerticalOffset: 1
            autoPaddingEnabled: true
        }

        Shared.AstreaAppIcon {
            anchors.fill: parent
            iconName: root.iconName
            iconPath: root.iconPath
            appName: root.appName
            maximumPresentationScale: root.maximumPresentationScale
            iconRadius: width * 0.225
            fallbackRadius: width * 0.225
        }
    }
}
