import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Effects
import "../../components" as Components
import "../../components/form" as Form

Item {
    id: root
    objectName: "wallpaperPage"

    readonly property var controller: SettingsController.wallpaper
    readonly property var transitions: [
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.simple", "Simple"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.fade", "Fade"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.left", "Left"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.right", "Right"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.top", "Top"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.bottom", "Bottom"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.wipe", "Wipe"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.wave", "Wave"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.grow", "Grow"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.center", "Center"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.outer", "Outer"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.any", "Any"),
        I18n.tr("apps.settings.pages.appearance.wallpaper.transition.random", "Random")
    ]
    property int selectedTransition: 0

    signal wallpaperImportRequested()
    signal futurePageRequested(string pageId)

    Component.onCompleted: root.controller.refreshLibrary()

    Form.ScrollPage {
        id: scrollPage
        objectName: "wallpaperScrollPage"
        anchors.fill: parent
        contentMargins: 28

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.wallpaper.current", "CURRENT")
            Layout.bottomMargin: 12
        }

        Rectangle {
            id: currentWallpaperCard
            objectName: "currentWallpaperCard"
            Layout.fillWidth: true
            Layout.bottomMargin: 8
            implicitHeight: currentWallpaperRow.implicitHeight + 32
            radius: 12
            color: Components.Theme.cardBg
            border.width: 1
            border.color: Components.Theme.cardBorder

            RowLayout {
                id: currentWallpaperRow
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: 16
                }
                spacing: 16

                Item {
                    id: wallpaperPreview
                    objectName: "wallpaperPreview"
                    Layout.preferredWidth: 180
                    Layout.preferredHeight: 112
                    width: 180
                    height: 112

                    Rectangle {
                        anchors.fill: parent
                        radius: 14
                        color: Components.Theme.cardBg
                        border.width: 1
                        border.color: Components.Theme.cardBorder
                    }

                    Item {
                        id: previewMask
                        anchors.fill: parent
                        visible: false
                        layer.enabled: true

                        Rectangle {
                            anchors.fill: parent
                            radius: 14
                        }
                    }

                    Image {
                        id: previewImage
                        anchors.fill: parent
                        source: root.controller.effectiveSource
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        smooth: true
                        mipmap: true
                        cache: false
                        retainWhileLoading: true
                        sourceSize.width: width
                        sourceSize.height: height
                        layer.enabled: true
                        layer.smooth: true
                        layer.mipmap: true
                        layer.effect: MultiEffect {
                            maskEnabled: true
                            maskSource: previewMask
                            maskThresholdMin: 0.4
                            maskSpreadAtMin: 0.6
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 4
                            visible: previewImage.status === Image.Error

                            Text {
                                Layout.alignment: Qt.AlignCenter
                                text: "\uf03e"
                                font.family: "JetBrainsMono Nerd Font"
                                font.pixelSize: 22
                                color: Components.Theme.textSecondary
                            }
                            Text {
                                Layout.alignment: Qt.AlignCenter
                                text: I18n.tr("apps.settings.pages.appearance.wallpaper.preview_fail", "Preview Fail")
                                font.family: Components.Theme.fontFamily
                                font.pixelSize: 10
                                font.weight: Font.Medium
                                color: Components.Theme.textSecondary
                            }
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 14
                        color: Qt.rgba(0, 0, 0, previewMouse.containsMouse ? 0.45 : 0)
                        Behavior on color { ColorAnimation { duration: 150 } }

                        Column {
                            anchors.centerIn: parent
                            spacing: 4
                            visible: previewMouse.containsMouse

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "\uf574"
                                font.family: "JetBrainsMono Nerd Font"
                                font.pixelSize: 22
                                color: "#ffffff"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: I18n.tr("apps.settings.pages.appearance.wallpaper.change", "Change")
                                font.family: Components.Theme.fontFamily
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                color: "#ffffff"
                            }
                        }
                    }

                    MouseArea {
                        id: previewMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.wallpaperImportRequested()
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: root.controller.currentDisplayName !== ""
                              ? root.controller.currentDisplayName
                              : I18n.tr("apps.settings.pages.appearance.wallpaper.my_wallpaper", "My Wallpaper")
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        color: Components.Theme.textPrimary
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.fillWidth: true
                            text: I18n.tr("apps.settings.pages.appearance.wallpaper.show_on_all_workspaces", "Show on all workspaces")
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: 13
                            color: Components.Theme.textPrimary
                        }
                        Form.ToggleSwitch {
                            id: allWorkspacesToggle
                            checked: true
                            onToggled: checked = true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.fillWidth: true
                            text: I18n.tr("apps.settings.pages.appearance.wallpaper.use_blurred_wallpaper", "Use blurred wallpaper")
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: 13
                            color: Components.Theme.textPrimary
                        }
                        Form.ToggleSwitch {
                            id: blurredWallpaperToggle
                            checked: false
                            onToggled: checked = false
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Repeater {
                            model: [
                                { id: "screensaver", label: I18n.tr("apps.settings.pages.appearance.wallpaper.screensaver", "Screensaver") },
                                { id: "lockscreen", label: I18n.tr("apps.settings.pages.appearance.wallpaper.lockscreen", "Lockscreen") }
                            ]

                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                height: 30
                                radius: 8
                                color: pageButtonMouse.containsMouse
                                       ? Qt.rgba(1, 1, 1, 0.08)
                                       : Qt.rgba(1, 1, 1, 0.04)
                                border.width: 1
                                border.color: Components.Theme.cardBorder
                                Behavior on color { ColorAnimation { duration: 120 } }

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    font.family: Components.Theme.fontFamily
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    color: Components.Theme.textPrimary
                                }

                                MouseArea {
                                    id: pageButtonMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.futurePageRequested(modelData.id)
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: transitionCard
            objectName: "transitionCard"
            Layout.fillWidth: true
            Layout.bottomMargin: 28
            implicitHeight: transitionRow.implicitHeight
            radius: 12
            color: Components.Theme.cardBg
            border.width: 1
            border.color: Components.Theme.cardBorder

            Form.SettingRow {
                id: transitionRow
                anchors.left: parent.left
                anchors.right: parent.right
                label: I18n.tr("apps.settings.pages.appearance.wallpaper.transition", "Transition")
                sublabel: I18n.tr("apps.settings.pages.appearance.wallpaper.transition_help", "awww wallpaper animation")
                isLast: true

                Form.SelectButton {
                    implicitWidth: 140
                    label: root.transitions[root.selectedTransition]
                    options: root.transitions
                    selectedIndex: root.selectedTransition
                    onSelected: index => root.selectedTransition = index
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.bottomMargin: 24
            height: 1
            color: Components.Theme.cardBorder
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.appearance.wallpaper.library", "WALLPAPER LIBRARY")
            Layout.bottomMargin: 12
        }

        Rectangle {
            id: wallpaperLibraryCard
            objectName: "wallpaperLibraryCard"
            Layout.fillWidth: true
            implicitHeight: libraryColumn.implicitHeight
            radius: 12
            color: Components.Theme.cardBg
            border.width: 1
            border.color: Components.Theme.cardBorder

            ColumnLayout {
                id: libraryColumn
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 0

                WallpaperSection {
                    objectName: "dynamicWallpapersSection"
                    title: I18n.tr("apps.settings.pages.appearance.wallpaper.dynamic_wallpapers", "Dynamic Wallpapers")
                    wallpapers: root.controller.dynamicWallpapers
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Components.Theme.cardBorder
                }

                WallpaperSection {
                    objectName: "userWallpapersSection"
                    title: I18n.tr("apps.settings.pages.appearance.wallpaper.user_wallpapers", "User Wallpapers")
                    wallpapers: root.controller.userWallpapers
                    showAddButton: true
                    onAddRequested: root.wallpaperImportRequested()
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Components.Theme.cardBorder
                }

                WallpaperSection {
                    objectName: "landscapesSection"
                    title: I18n.tr("apps.settings.pages.appearance.wallpaper.landscapes", "Landscapes")
                    wallpapers: root.controller.landscapeWallpapers
                }
            }
        }

        Item { Layout.preferredHeight: 28 }
    }

    component WallpaperSection: ColumnLayout {
        id: section
        property string title: ""
        property var wallpapers: []
        property bool showAddButton: false
        property bool open: true
        signal addRequested()
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 12
            Layout.bottomMargin: 12
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: section.title
                font.family: Components.Theme.fontFamily
                font.pixelSize: 13
                font.weight: Font.Medium
                color: Components.Theme.textPrimary
            }

            Rectangle {
                visible: section.showAddButton
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                radius: 8
                color: addButtonMouse.containsMouse
                       ? Qt.rgba(Components.Theme.accent.r, Components.Theme.accent.g, Components.Theme.accent.b, 0.15)
                       : Qt.rgba(1, 1, 1, 0.06)
                border.width: 1
                border.color: addButtonMouse.containsMouse ? Components.Theme.accent : Components.Theme.cardBorder
                Behavior on color { ColorAnimation { duration: 120 } }

                Text {
                    anchors.centerIn: parent
                    text: "+"
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: 16
                    font.weight: Font.Light
                    color: addButtonMouse.containsMouse ? Components.Theme.accent : Components.Theme.textSecondary
                }

                MouseArea {
                    id: addButtonMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: section.addRequested()
                }
            }

            Item {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 26

                Text {
                    anchors.centerIn: parent
                    text: section.open ? "▾" : "▸"
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: 11
                    color: Components.Theme.textSecondary
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: section.open = !section.open
                }
            }
        }

        Item {
            visible: section.open
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.bottomMargin: 14
            implicitHeight: section.wallpapers.length > 0 ? wallpaperGrid.implicitHeight : emptyLabel.implicitHeight

            Text {
                id: emptyLabel
                anchors.horizontalCenter: parent.horizontalCenter
                text: I18n.tr("apps.settings.pages.appearance.wallpaper.no_wallpapers_found", "No wallpapers found")
                font.family: Components.Theme.fontFamily
                font.pixelSize: 12
                color: Components.Theme.textSecondary
                visible: section.wallpapers.length === 0
            }

            Grid {
                id: wallpaperGrid
                width: parent.width
                columns: 3
                spacing: 8
                visible: section.wallpapers.length > 0

                Repeater {
                    model: section.wallpapers

                    delegate: Item {
                        required property var modelData
                        width: (wallpaperGrid.width - 16) / 3
                        height: width * 0.6 + 28

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 6

                            Rectangle {
                                id: tileImage
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 14
                                color: Qt.rgba(0.05, 0.11, 0.16, 1)
                                border.width: 1
                                border.color: tileMouse.containsMouse ? Components.Theme.accent : Components.Theme.cardBorder
                                Behavior on border.color { ColorAnimation { duration: 120 } }

                                Item {
                                    id: tileMask
                                    anchors.fill: parent
                                    visible: false
                                    layer.enabled: true

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 14
                                    }
                                }

                                Image {
                                    anchors.fill: parent
                                    source: modelData.resolvedSource || modelData.source || ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    smooth: true
                                    mipmap: true
                                    cache: true
                                    layer.enabled: true
                                    layer.effect: MultiEffect {
                                        maskEnabled: true
                                        maskSource: tileMask
                                    }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.displayName || modelData.logicalId || "Wallpaper"
                                font.family: Components.Theme.fontFamily
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                color: Components.Theme.textPrimary
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        MouseArea {
                            id: tileMouse
                            anchors.fill: parent
                            enabled: !root.controller.busy && (modelData.kind || "image") === "image"
                            hoverEnabled: true
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: root.controller.selectWallpaper(
                                           modelData.logicalId,
                                           root.controller.effectiveFit !== "" ? root.controller.effectiveFit : "cover")
                        }
                    }
                }
            }
        }
    }
}
