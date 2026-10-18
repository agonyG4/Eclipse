import QtQuick
import Qt5Compat.GraphicalEffects as Legacy
import QtQuick.Layouts
import QtQuick.Effects
import ".." as Components

Item {
    id: root

    height: 40

    required property string label
    property string sym: ""
    property string iconSource: ""
    // iconKey is retained for compatibility with existing callers; composed
    // navigation supplies the resolved source instead.
    property string iconKey: ""
    required property bool   selected
    signal clicked()

    FontLoader { id: tablerIcons; source: "qrc:/Astrea/Settings/assets/fonts/tabler-icons-300.ttf" }

    readonly property color accent: Components.Theme.accent
    readonly property bool isLight: Components.Theme.themeMode === 1
    readonly property color idleForeground: isLight ? Components.Theme.textSecondary : Qt.rgba(1, 1, 1, 0.78)
    readonly property color accentForeground: Components.Theme.accentForeground
    readonly property color activeForeground: Components.Theme.textPrimary
    // ── Fundo ─────────────────────────────────────────────────────────────
    Rectangle {
        id: bgRect
        anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
        radius: 8
        color: root.selected
            ? Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.12)
            : hma.containsMouse ? (root.isLight ? Qt.rgba(0, 0, 0, 0.045) : Qt.rgba(1, 1, 1, 0.05)) : "transparent"
        border.width: root.selected ? 1 : (hma.containsMouse ? 1 : 0)
        border.color: root.selected ? Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.25) : (root.isLight ? Qt.rgba(0, 0, 0, 0.06) : Qt.rgba(1, 1, 1, 0.05))

        Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }

        // Subtle left indicator for selected state
        Rectangle {
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
            width: 3
            height: root.selected ? parent.height * 0.5 : 0
            radius: 1.5
            color: root.accent
            opacity: root.selected ? 1.0 : 0.0
            Behavior on height { NumberAnimation { duration: 250; easing.type: Easing.OutBack } }
            Behavior on opacity { NumberAnimation { duration: 200 } }
        }
    }

    // ── Conteúdo ──────────────────────────────────────────────────────────
    RowLayout {
        anchors { fill: parent; leftMargin: 16; rightMargin: 12 }
        spacing: 12

        Rectangle {
            width: 28
            height: 28
            radius: 8
            color: root.selected ? root.accent : (hma.containsMouse ? (root.isLight ? Qt.rgba(0, 0, 0, 0.07) : Qt.rgba(1, 1, 1, 0.1)) : (root.isLight ? Qt.rgba(0, 0, 0, 0.04) : Qt.rgba(1, 1, 1, 0.05)))
            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutCubic } }

            // Adds a gentle inner shadow / highlight effect overlay
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.width: 1
                border.color: root.isLight ? Qt.rgba(0, 0, 0, root.selected ? 0.08 : 0.05) : Qt.rgba(1, 1, 1, root.selected ? 0.2 : 0.08)
            }

            Text {
                anchors.centerIn: parent
                text:           root.sym
                color:          root.selected ? root.accentForeground : (hma.containsMouse ? root.activeForeground : root.idleForeground)
                font.pixelSize: 18
                font.family: tablerIcons.name
                visible:        root.iconSource === ""
                Behavior on color { ColorAnimation { duration: 150 } }

                // Active glow for the icon
                layer.enabled: root.selected && visible
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: root.accent
                    shadowBlur: 0.8
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 0
                }
            }

            Image {
                id: navIcon
                objectName: "navigationThemedIcon"
                anchors.centerIn: parent
                width: 18
                height: 18

                readonly property string resolvedSource: root.iconSource
                readonly property color resolvedTint: root.selected
                    ? root.accentForeground : root.activeForeground

                source: resolvedSource
                visible: resolvedSource !== ""

                // Render the SVG at its displayed size. Downsampling a 2x image
                // with mipmaps and another offscreen effect softened fine outlines.
                sourceSize: Qt.size(width, height)
                fillMode: Image.PreserveAspectFit
                mipmap: false
                smooth: true

                // SVG icons may be black or white depending on their source.
                // Always tint the silhouette to the semantic foreground,
                // including dark mode and selected navigation rows.
                layer.enabled: visible
                layer.effect: Legacy.ColorOverlay {
                    color: navIcon.resolvedTint
                }
            }
        }

        Text {
            text:        root.label
            color:       root.selected ? root.activeForeground : (hma.containsMouse ? root.activeForeground : root.idleForeground)
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeNormal
            font.weight: root.selected ? Components.Theme.fontWeightDemiBold : Components.Theme.fontWeightMedium
            elide:       Text.ElideRight
            Layout.fillWidth: true
            scale: hma.pressed ? 0.97 : 1.0
            Behavior on color { ColorAnimation { duration: 150 } }
            Behavior on scale { NumberAnimation { duration: 100 } }
        }
    }

    // ── Interação ─────────────────────────────────────────────────────────
    MouseArea {
        id: hma
        anchors.fill: parent
        hoverEnabled: true
        enabled:      root.enabled
        cursorShape:  root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked:    root.clicked()
    }
}
