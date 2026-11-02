import QtQuick
import QtQuick.Layouts
import ".." as Components
import "../controls" as Controls

Item {
    id: root
    objectName: root.destinationId !== "" ? "hubNavigationRow-" + root.destinationId : "hubNavigationRow"

    property string destinationId: ""
    property string label: ""
    property string sublabel: ""
    property string iconSource: ""
    property string sym: ""
    FontLoader { id: tablerIcons; source: "qrc:/Astrea/Settings/assets/fonts/tabler-icons-300.ttf" }

    readonly property string chevronGlyph: "\u203A"
    property bool isLast: false
    signal clicked()

    Layout.fillWidth: true
    implicitHeight: sublabel !== "" ? 68 : 56

    Rectangle {
        anchors.fill: parent
        anchors.margins: Components.Theme.spacingTiny
        radius: Components.Theme.cornerRadiusSmall
        color: rowMouse.containsMouse
            ? (Components.Theme.isLight ? Qt.rgba(0, 0, 0, 0.045) : Qt.rgba(1, 1, 1, 0.05))
            : "transparent"
        border.width: rowMouse.containsMouse ? 1 : 0
        border.color: Components.Theme.isLight
            ? Qt.rgba(0, 0, 0, 0.06)
            : Qt.rgba(1, 1, 1, 0.05)

        Behavior on color { ColorAnimation { duration: Components.Theme.animationFast } }
        Behavior on border.color { ColorAnimation { duration: Components.Theme.animationFast } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Components.Theme.spacingLarge
        anchors.rightMargin: Components.Theme.spacingLarge
        spacing: Components.Theme.spacingMedium

        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            width: 32
            height: 32
            radius: Components.Theme.radiusSmall
            color: rowMouse.containsMouse
                ? Qt.rgba(1, 1, 1, 0.10)
                : Qt.rgba(1, 1, 1, 0.055)

            Controls.ThemedIcon {
                anchors.centerIn: parent
                width: 20
                height: 20
                iconSource: root.iconSource
                tint: Components.Theme.textPrimary
                visible: root.iconSource !== ""
                opacity: root.enabled ? 1 : Components.Theme.opacityMuted
            }

            Text {
                anchors.centerIn: parent
                text: root.sym
                visible: root.iconSource === "" && root.sym !== ""
                color: Components.Theme.textSecondary
                font.family: tablerIcons.name
                font.pixelSize: 20
                opacity: root.enabled ? 1 : Components.Theme.opacityMuted
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.label
                color: Components.Theme.textPrimary
                font.family: Components.Theme.typography.role("label").family
                font.pixelSize: Components.Theme.typography.role("label").pixelSize
                lineHeight: Components.Theme.typography.role("label").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("label").letterSpacing
                font.weight: Components.Theme.typography.role("label").weight
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                visible: root.sublabel !== ""
                text: root.sublabel
                color: Components.Theme.textSecondary
                font.family: Components.Theme.typography.role("caption").family
                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                lineHeight: Components.Theme.typography.role("caption").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                opacity: Components.Theme.opacitySecondary
                elide: Text.ElideRight
            }
        }

        Text {
            text: root.chevronGlyph
            color: Components.Theme.textTertiary
            font.family: Components.Theme.typography.role("label").family
            font.pixelSize: Components.Theme.typography.role("label").pixelSize
            lineHeight: Components.Theme.typography.role("label").lineHeight
            lineHeightMode: Text.ProportionalHeight
            font.letterSpacing: Components.Theme.typography.role("label").letterSpacing
            opacity: root.enabled ? 0.72 : Components.Theme.opacityDisabled
        }
    }

    Rectangle {
        visible: !root.isLast
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Components.Theme.spacingLarge + 32 + Components.Theme.spacingMedium
        anchors.rightMargin: Components.Theme.spacingLarge
        height: 1
        color: Components.Theme.cardBorder
        opacity: Components.Theme.opacityDisabled
    }

    MouseArea {
        id: rowMouse
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
