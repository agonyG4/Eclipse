import QtQuick
import QtQuick.Window
import "components"

Window {
    id: window

    title: qsTr("Astrea Dock")
    // Output geometry is supplied by the C++ layer-shell surface. The Dock
    // window is a fixed transparent envelope; these properties describe the
    // output-local coordinate space used by its context-menu anchors.
    property string outputKey: ""
    property int outputWidth: 1
    property int outputHeight: 1
    property int outputOriginX: 0
    property int outputOriginY: 0
    readonly property var windowPreviewService:
        typeof DockWindowPreviewService === "undefined" ? null : DockWindowPreviewService
    readonly property var windowPreviewInputRegion:
        typeof DockWindowPreviewInputRegion === "undefined" ? null : DockWindowPreviewInputRegion
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint
    width: panel.surfaceWidth
    height: panel.surfaceHeight

    DockPanel {
        id: panel
        objectName: "dockPanel"
        anchors.centerIn: parent
        contextMenuController: ContextMenuController
        dockSurfaceGeometry: DockSurfaceGeometry
        dockTooltipSurface: tooltipSurface
        dockWindowPreviewSurface: windowPreviewSurface
        dockWindowPreviewService: window.windowPreviewService
        inputRegionBridge: DockInputRegion
        outputKey: window.outputKey
        outputWidth: window.outputWidth
        outputHeight: window.outputHeight
        outputOriginX: window.outputOriginX
        outputOriginY: window.outputOriginY
        onReorderRequested: function(desktopFileName, targetPinIndex) {
            DockController.movePinned(desktopFileName, targetPinIndex)
        }
    }

    DockTooltipSurface {
        id: tooltipSurface
        outputKey: window.outputKey
        outputWidth: window.outputWidth
        outputHeight: window.outputHeight
        outputOriginX: window.outputOriginX
        outputOriginY: window.outputOriginY
    }

    DockWindowPreviewSurface {
        id: windowPreviewSurface
        outputKey: window.outputKey
        outputWidth: window.outputWidth
        outputHeight: window.outputHeight
        outputOriginX: window.outputOriginX
        outputOriginY: window.outputOriginY
        dockPanel: panel
        previewService: window.windowPreviewService
        dockSurfaceGeometry: DockSurfaceGeometry
        inputRegionBridge: window.windowPreviewInputRegion
    }

}
