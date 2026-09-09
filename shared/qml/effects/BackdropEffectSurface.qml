import QtQuick
import QtQuick.Window
import Astrea.Effects

Item {
    id: root

    property Component content
    property real cornerRadius: 18
    property bool effectEnabled: true
    readonly property bool effectAvailable: childWindow.effectAvailable
    readonly property bool effectActive: childWindow.effectActive

    WindowContainer {
        id: container
        anchors.fill: parent
        window: AstreaEffectChildWindow {
            id: childWindow
            width: root.width
            height: root.height
            visible: root.visible && root.effectEnabled
            effectEnabled: root.effectEnabled
            cornerRadius: root.cornerRadius

            Loader {
                id: contentLoader
                parent: childWindow.contentItem
                anchors.fill: parent
                sourceComponent: root.content
            }
        }
    }
}
