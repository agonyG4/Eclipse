import QtQuick
import QtQuick.Window
import "components"

Window {
    id: window

    property var barController: null
    property var popupController: null
    property var workspaceModel: null
    property var barGeometry: null
    property int outputWidth: 1
    property int outputHeight: 1
    property int leftMargin: barGeometry ? barGeometry.launcherLeftMargin : 8
    property string logoSource: "qrc:/qt/qml/Astrea/Shell/Bar/assets/astrea.png"
    readonly property int launcherVisualWidth: Math.max(48, launcherPill.width)
    readonly property int launcherSurfaceWidth: Math.max(
        launcherVisualWidth,
        launcherPill.horizontalPadding * 2 + logoButton.width
            + launcherPill.spacing + workspaceStrip.reservedWidth)
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: launcherSurfaceWidth
    height: barGeometry ? barGeometry.pillHeight : 36

    ShellBarTheme { id: theme }

    BarSegment {
        id: launcherPill
        objectName: "launcherPill"
        interactive: false
        spacing: theme.spacing
        horizontalPadding: barGeometry ? barGeometry.sidePadding : 10

        TopbarIndicator {
            id: logoButton
            objectName: "logoButton"
            popupController: window.popupController
            popupKind: 1
            anchorItem: launcherPill
            anchorOffset: barGeometry ? barGeometry.launcherLeftMargin : 8
            fixedWidth: 28
            height: 28
            backgroundMargin: 0
            backgroundRadius: theme.shellRadiusMedium

            Image {
                id: logoImage
                objectName: "logoImage"
                source: window.logoSource
                sourceSize.width: 18
                sourceSize.height: 18
                width: 18
                height: 18
                fillMode: Image.PreserveAspectFit
                smooth: true
                opacity: theme.opacityMuted
            }
        }

        WorkspaceStrip {
            id: workspaceStrip
            objectName: "workspaceStrip"
            workspaceModel: window.workspaceModel
            activationAvailable: false
        }
    }
}
