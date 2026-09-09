import QtQuick
import "../../components" as Components

Item {
    id: root
    property string themeVariant: "dark"
    property string materialId: "default"

    readonly property real materialOpacity: materialId === "transparent" ? 0.58
        : materialId === "frosted" ? 0.80 : 0.96
    readonly property color lightSurface: "#fbfcff"
    readonly property color lightText: "#4c5665"
    readonly property color darkSurface: "#272e3a"
    readonly property color darkText: "#d8deea"

    Rectangle {
        id: showcase
        objectName: "materialPreviewShowcase"
        anchors.centerIn: parent
        width: parent.width * 0.70
        height: parent.height * 0.64
        radius: 8
        clip: true
        color: root.themeVariant === "auto"
            ? "transparent"
            : root.themeVariant === "light"
                ? root.lightSurface
                : root.themeVariant === "dark"
                    ? root.darkSurface
                    : Components.Theme.isLight ? root.lightSurface : root.darkSurface
        opacity: root.themeVariant === "auto" ? 0.96 : root.materialOpacity
        border.width: 1
        border.color: root.themeVariant === "auto"
            ? "#9aa8b9"
            : root.themeVariant === "light" ? "#d2d9e4" : "#3b4554"

        Rectangle {
            visible: root.themeVariant === "auto"
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: parent.width / 2
            color: root.lightSurface
        }

        Rectangle {
            visible: root.themeVariant === "auto"
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: parent.width / 2
            color: root.darkSurface
        }

        Row {
            visible: root.themeVariant !== "auto"
            x: 8
            y: 7
            spacing: 4

            Repeater {
                model: 3
                delegate: Rectangle {
                    width: 5
                    height: 5
                    radius: 3
                    color: root.themeVariant === "light" ? root.lightText
                        : root.themeVariant === "dark" ? root.darkText
                            : Components.Theme.isLight ? root.lightText : root.darkText
                    opacity: 0.72
                }
            }
        }

        Rectangle {
            visible: root.themeVariant !== "auto"
            x: 10
            y: 24
            width: parent.width * 0.43
            height: 5
            radius: 2
            color: root.themeVariant === "light" ? root.lightText
                : root.themeVariant === "dark" ? root.darkText
                    : Components.Theme.isLight ? root.lightText : root.darkText
            opacity: 0.52
        }

        Rectangle {
            visible: root.themeVariant !== "auto"
            x: 10
            y: 36
            width: parent.width * 0.66
            height: 4
            radius: 2
            color: root.themeVariant === "light" ? root.lightText
                : root.themeVariant === "dark" ? root.darkText
                    : Components.Theme.isLight ? root.lightText : root.darkText
            opacity: 0.28
        }

        Item {
            visible: root.themeVariant === "auto"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width / 2
            clip: true

            Row {
                x: 8
                y: 7
                spacing: 4

                Repeater {
                    model: 3
                    delegate: Rectangle {
                        width: 5
                        height: 5
                        radius: 3
                        color: root.lightText
                        opacity: 0.72
                    }
                }
            }

            Rectangle {
                x: 10
                y: 24
                width: parent.width * 0.72
                height: 5
                radius: 2
                color: root.lightText
                opacity: 0.52
            }

            Rectangle {
                x: 10
                y: 36
                width: parent.width * 0.88
                height: 4
                radius: 2
                color: root.lightText
                opacity: 0.28
            }
        }

        Item {
            visible: root.themeVariant === "auto"
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width / 2
            clip: true

            Row {
                x: 8
                y: 7
                spacing: 4

                Repeater {
                    model: 3
                    delegate: Rectangle {
                        width: 5
                        height: 5
                        radius: 3
                        color: root.darkText
                        opacity: 0.72
                    }
                }
            }

            Rectangle {
                x: 10
                y: 24
                width: parent.width * 0.72
                height: 5
                radius: 2
                color: root.darkText
                opacity: 0.52
            }

            Rectangle {
                x: 10
                y: 36
                width: parent.width * 0.88
                height: 4
                radius: 2
                color: root.darkText
                opacity: 0.28
            }
        }

        Rectangle {
            visible: root.themeVariant !== "auto"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 12
            color: root.themeVariant === "light" ? "#d1dce9"
                : root.themeVariant === "dark" ? "#3a4657"
                    : Components.Theme.isLight ? "#d1dce9" : "#3a4657"
            opacity: root.materialId === "transparent" ? 0.42
                : root.materialId === "frosted" ? 0.65 : 0.90
        }

        Rectangle {
            visible: root.themeVariant !== "auto"
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 10
            anchors.bottomMargin: 8
            width: 18
            height: 3
            radius: 2
            color: Components.Theme.accent
            opacity: 0.72
        }
    }
}
