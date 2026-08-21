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
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: Math.max(48, launcherPill.width)
    height: barGeometry ? barGeometry.pillHeight : 36

    ShellBarTheme { id: theme }

    BarSegment {
        id: launcherPill
        objectName: "launcherPill"
        interactive: false
        horizontalPadding: barGeometry ? barGeometry.sidePadding : 10

        TopbarIndicator {
            id: logoButton
            objectName: "logoButton"
            popupController: window.popupController
            popupKind: 1
            anchorItem: launcherPill
            anchorOffset: barGeometry ? barGeometry.launcherLeftMargin : 8
            fixedWidth: 28
            height: 34
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
            workspaceModel: window.workspaceModel
            activationAvailable: window.barController && window.barController.workspaceController
                ? window.barController.workspaceController.activationAvailable : false
            onWorkspaceActivated: workspaceId => {
                if (window.barController && window.barController.workspaceController)
                    window.barController.workspaceController.activateWorkspace(workspaceId)
            }
        }
    }
}
