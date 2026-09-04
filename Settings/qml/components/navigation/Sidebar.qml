import QtQuick
import QtQuick.Layouts
import ".." as Components

Item {
    id: root
    objectName: "settingsSidebar"

    required property var   model
    required property string selectedId
    property var translationMessages: ({})
    signal selectId(string id)

    property string userName: ""
    property string avatarPath: ""
    property bool isSudo: false
    property string iconTheme: ""
    property var iconUrlResolver: null

    function translatedLabel(item) {
        const key = item.labelKey !== undefined ? item.labelKey : ""
        if (key.length > 0 && root.translationMessages && root.translationMessages[key])
            return root.translationMessages[key]
        return item.label || ""
    }

    Components.SidebarFrame {
        anchors.fill: parent
        backgroundColor: Components.Theme.themeMode === 1
            ? (Components.Theme.shellStyle === 0 ? Qt.rgba(1, 1, 1, 0.18)
                : Components.Theme.shellStyle === 2 ? Qt.rgba(0.98, 0.99, 1, 0.28)
                : Qt.rgba(0.985, 0.987, 0.994, 0.96))
            : Qt.rgba(1, 1, 1, 0.05)
        washColor: Components.Theme.themeMode === 1
            ? (Components.Theme.shellStyle === 0 ? Qt.rgba(1, 1, 1, 0.04)
                : Components.Theme.shellStyle === 2 ? Qt.rgba(1, 1, 1, 0.10)
                : Qt.rgba(1, 1, 1, 0.04))
            : Qt.rgba(1, 1, 1, 0.015)
        borderColor: Components.Theme.themeMode === 1
            ? (Components.Theme.shellStyle === 0 ? Qt.rgba(0, 0, 0, 0.08)
                : Components.Theme.shellStyle === 2 ? Qt.rgba(0, 0, 0, 0.10)
                : Qt.rgba(0, 0, 0, 0.09))
            : Qt.rgba(1, 1, 1, 0.08)
        contentTopPadding: 16
        contentBottomPadding: 16
        contentSpacing: 2

        // ── Profile header ────────────────────────────────────────────────
        Item {
            width: parent.width - 32
            x: 16
            height: 64

            RowLayout {
                anchors.fill: parent
                spacing: 12

                // Avatar
                Item {
                    Layout.alignment: Qt.AlignVCenter
                    width: 48
                    height: 48

                    Components.AvatarImage {
                        anchors.fill: parent
                        imagePath: root.avatarPath
                        imageVersion: 0
                        fallbackText: root.userName.length > 0
                            ? root.userName[0].toUpperCase()
                            : "?"
                        fallbackFontFamily: Components.Theme.fontFamily
                        fallbackFontPixelSize: Components.Theme.fontSizeAvatar
                        fallbackFontWeight: Components.Theme.fontWeightMedium
                        sourceScale: 4
                        maskMargin: 1
                        borderWidth: 1
                        borderColor: Qt.rgba(1, 1, 1, 0.18)
                    }
                }

                // Nome e Sudo badge
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: root.userName
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeLarge
                        font.weight: Components.Theme.fontWeightMedium
                        color: Components.Theme.textPrimary
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "sudo"
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeTiny
                        font.weight: Components.Theme.fontWeightNormal
                        color: Components.Theme.textTertiary
                        visible: root.isSudo
                    }
                }
            }
        }

        // Divisor
        Rectangle {
            width: parent.width - 32
            x: 16
            height: 1
            color: Components.Theme.cardBorder
        }

        Item { width: 1; height: 4 }

        // ── Nav items ─────────────────────────────────────────────────────
        Repeater {
            model: root.model
            delegate: Item {
                id: navDelegate

                readonly property string itemKind: model.kind !== undefined && model.kind.length > 0 ? model.kind : "page"

                width: parent.width
                height: itemKind === "spacer" ? 12
                    : itemKind === "section" ? 32
                    : itemKind === "child" ? 36
                    : 40
                visible: height > 0
                clip: true

                Behavior on height { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                Components.NavItem {
                    anchors.fill: parent
                    visible:    navDelegate.itemKind !== "section" && navDelegate.itemKind !== "spacer"
                    label:      root.translatedLabel(model)
                    sym:        model.sym !== undefined ? model.sym : ""
                    iconSource: model.iconKey !== undefined && root.iconUrlResolver
                        ? root.iconUrlResolver(model.iconKey, root.iconTheme)
                        : (model.iconSource !== undefined ? model.iconSource : "")
                    iconKey: ""
                    leftInset:  navDelegate.itemKind === "child" ? 24 : 0
                    compact:    navDelegate.itemKind === "child"
                    enabled:    model.entryEnabled === true
                    selected:   model.entryEnabled === true && root.selectedId === model.entryId
                    onClicked:  root.selectId(model.entryId)
                }

                Rectangle {
                    visible: navDelegate.itemKind === "section"
                    anchors {
                        fill: parent
                        leftMargin: 8
                        rightMargin: 8
                        topMargin: 3
                        bottomMargin: 3
                    }
                    radius: 8
                    color: sectionMouse.containsMouse
                        ? (Components.Theme.themeMode === 1
                            ? Qt.rgba(0, 0, 0, 0.04)
                            : Qt.rgba(1, 1, 1, 0.045))
                        : "transparent"
                    border.width: sectionMouse.containsMouse ? 1 : 0
                    border.color: Components.Theme.themeMode === 1
                        ? Qt.rgba(0, 0, 0, 0.055)
                        : Qt.rgba(1, 1, 1, 0.05)

                    RowLayout {
                        anchors {
                            fill: parent
                            leftMargin: 12
                            rightMargin: 10
                        }
                        spacing: 8

                        Text {
                            text: model.expanded === true ? "⌄" : "›"
                            color: Components.Theme.textTertiary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeSmall
                            horizontalAlignment: Text.AlignHCenter
                            Layout.preferredWidth: 14
                        }

                        Text {
                            text: root.translatedLabel(model)
                            color: Components.Theme.textSecondary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeSmall
                            font.weight: Components.Theme.fontWeightDemiBold
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    MouseArea {
                        id: sectionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.model.toggleSection(model.entryId)
                    }
                }
            }
        }

        Item { width: 1; height: 8 }
    }
}
