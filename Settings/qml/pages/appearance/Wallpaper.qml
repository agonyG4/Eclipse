import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Effects
import "../../components" as Components
import "../../components/form" as Form

Item {
    id: root
    objectName: "wallpaperPage"

    readonly property var controller: SettingsController.wallpaper
    readonly property var transitions: [
        I18n.tr("apps.settings.pages.paper.wallpaper.option.simple", "Simple"),
        I18n.tr("apps.settings.pages.paper.wallpaper.option.fade", "Fade"),
        I18n.tr("apps.settings.pages.paper.wallpaper.option.left", "Left"),
        I18n.tr("apps.settings.pages.paper.wallpaper.option.right", "Right"),
        I18n.tr("apps.settings.pages.paper.wallpaper.option.top", "Top"),
        I18n.tr("apps.settings.pages.paper.wallpaper.option.bottom", "Bottom"),
        I18n.tr("apps.settings.pages.paper.wallpaper.option.wipe", "Wipe"),
        I18n.tr("apps.settings.pages.paper.wallpaper.option.wave", "Wave"),
        I18n.tr("apps.settings.pages.paper.wallpaper.option.grow", "Grow"),
        I18n.tr("apps.settings.pages.paper.wallpaper.option.center", "Center"),
        I18n.tr("apps.settings.pages.paper.wallpaper.option.outer", "Outer"),
        I18n.tr("apps.settings.pages.paper.wallpaper.option.any", "Any"),
        I18n.tr("apps.settings.pages.paper.wallpaper.option.random", "Random")
    ]
    property int selectedTransition: 0
    property string pendingWallpaperPath: ""
    property bool pendingAddsToLibrary: false

    function openWallpaperPicker(addOnly) {
        if (root.controller.busy)
            return
        root.pendingAddsToLibrary = addOnly
        root.pendingWallpaperPath = ""
        wallpaperFileDialog.open()
    }

    Component.onCompleted: root.controller.refreshLibrary()

    Form.ScrollPage {
        id: scrollPage
        objectName: "wallpaperScrollPage"
        anchors.fill: parent
        contentMargins: 28

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.paper.wallpaper.text.current", "CURRENT")
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
                                text: I18n.tr("apps.settings.pages.paper.wallpaper.text.preview_fail", "Preview Fail")
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
                                text: I18n.tr("apps.settings.pages.paper.wallpaper.text.change", "Change")
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
                        enabled: !root.controller.busy
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: root.openWallpaperPicker(false)
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
                              : I18n.tr("apps.settings.pages.paper.wallpaper.text.my_wallpaper", "My Wallpaper")
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
                            text: I18n.tr("apps.settings.pages.paper.wallpaper.text.show_on_all_workspaces", "Show on all workspaces")
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
                            text: I18n.tr("apps.settings.pages.paper.wallpaper.text.use_blurred_wallpaper", "Use blurred wallpaper")
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
                                { id: "screensaver", label: I18n.tr("apps.settings.pages.paper.screensaver.text.screensaver", "Screensaver") },
                                { id: "lockscreen", label: I18n.tr("apps.settings.pages.paper.lockscreen.text.lockscreen", "Lockscreen") }
                            ]

                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                height: 30
                                radius: 8
                                color: Qt.rgba(1, 1, 1, 0.04)
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

                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: wallpaperFeedback
            objectName: "wallpaperFeedback"
            visible: root.controller.busy || root.controller.errorMessage !== ""
            Layout.fillWidth: true
            Layout.bottomMargin: 8
            implicitHeight: feedbackText.implicitHeight + 16
            radius: 8
            color: root.controller.errorMessage !== ""
                   ? Qt.rgba(0.85, 0.20, 0.20, 0.14)
                   : Qt.rgba(1, 1, 1, 0.06)
            border.width: 1
            border.color: root.controller.errorMessage !== ""
                          ? Qt.rgba(1, 0.35, 0.35, 0.32)
                          : Components.Theme.cardBorder

            Text {
                id: feedbackText
                anchors {
                    left: parent.left
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    margins: 8
                }
                text: root.controller.errorMessage !== ""
                      ? root.controller.errorMessage
                      : I18n.tr("apps.settings.pages.paper.wallpaper.text.operation_in_progress", "Applying wallpaper…")
                font.family: Components.Theme.fontFamily
                font.pixelSize: 11
                color: Components.Theme.textPrimary
                elide: Text.ElideRight
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
                label: I18n.tr("apps.settings.pages.paper.wallpaper.label.transition", "Transition")
                sublabel: I18n.tr("apps.settings.pages.paper.wallpaper.sublabel.awww_wallpaper_animation", "awww wallpaper animation")
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
            text: I18n.tr("apps.settings.pages.paper.wallpaper.text.wallpaper_library", "WALLPAPER LIBRARY")
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
                    title: I18n.tr("apps.settings.pages.paper.wallpaper.label.dynamic_wallpapers", "Dynamic Wallpapers")
                    wallpapers: root.controller.dynamicWallpapers
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Components.Theme.cardBorder
                }

                WallpaperSection {
                    objectName: "userWallpapersSection"
                    title: I18n.tr("apps.settings.pages.paper.wallpaper.text.user_wallpapers", "User Wallpapers")
                    wallpapers: root.controller.userWallpapers
                    showAddButton: true
                    onAddRequested: root.openWallpaperPicker(true)
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Components.Theme.cardBorder
                }

                WallpaperSection {
                    objectName: "landscapesSection"
                    title: I18n.tr("apps.settings.pages.paper.wallpaper.label.landscapes", "Landscapes")
                    wallpapers: root.controller.landscapeWallpapers
                }
            }
        }

        Item { Layout.preferredHeight: 28 }
    }

    FileDialog {
        id: wallpaperFileDialog
        objectName: "wallpaperFileDialog"
        title: I18n.tr("apps.settings.pages.paper.wallpaper.text.choose_wallpaper", "Choose a wallpaper")
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif)"]

        onAccepted: {
            const selectedPath = selectedFile.toString()
            if (selectedPath === "")
                return
            root.pendingWallpaperPath = selectedPath
            wallpaperNameDialog.open()
        }
        onRejected: {
            root.pendingWallpaperPath = ""
            root.pendingAddsToLibrary = false
        }
    }

    Dialog {
        id: wallpaperNameDialog
        objectName: "wallpaperNameDialog"
        modal: true
        width: 320
        padding: 20
        anchors.centerIn: Overlay.overlay
        Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.6) }

        background: Rectangle {
            radius: 14
            color: Components.Theme.cardBg
            border.width: 1
            border.color: Components.Theme.cardBorder
        }

        contentItem: ColumnLayout {
            spacing: 16

            Text {
                Layout.fillWidth: true
                text: I18n.tr("apps.settings.pages.paper.wallpaper.text.name_this_wallpaper", "Name this wallpaper")
                font.family: Components.Theme.fontFamily
                font.pixelSize: 15
                font.weight: Font.Medium
                color: Components.Theme.textPrimary
            }

            TextField {
                id: wallpaperNameInput
                objectName: "wallpaperNameInput"
                Layout.fillWidth: true
                height: 36
                placeholderText: I18n.tr("apps.settings.pages.paper.wallpaper.text.name_this_wallpaper", "Name this wallpaper")
                leftPadding: 12
                rightPadding: 12
                selectByMouse: true
                background: Rectangle {
                    radius: 8
                    color: Components.Theme.popupBg
                    border.width: 1
                    border.color: wallpaperNameInput.activeFocus
                                   ? Components.Theme.accent
                                   : Components.Theme.cardBorder
                }
            }
        }

        footer: RowLayout {
            spacing: 8

            Button {
                objectName: "wallpaperNameCancelButton"
                Layout.fillWidth: true
                implicitHeight: 34
                text: I18n.tr("apps.wallpapers.action.cancel", "Cancel")
                onClicked: wallpaperNameDialog.reject()
                background: Rectangle {
                    radius: 8
                    color: Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1
                    border.color: Components.Theme.cardBorder
                }
            }

            Button {
                objectName: "wallpaperNameAcceptButton"
                Layout.fillWidth: true
                implicitHeight: 34
                enabled: wallpaperNameInput.text.trim() !== ""
                text: I18n.tr("apps.settings.pages.paper.wallpaper.text.add", "Add")
                onClicked: wallpaperNameDialog.accept()
                background: Rectangle {
                    radius: 8
                    color: wallpaperNameInput.text.trim() !== ""
                           ? Components.Theme.accent
                           : Qt.rgba(1, 1, 1, 0.06)
                    border.width: 1
                    border.color: Components.Theme.accent
                }
            }
        }

        onOpened: wallpaperNameInput.forceActiveFocus()
        onAccepted: {
            const selectedPath = root.pendingWallpaperPath
            const displayName = wallpaperNameInput.text.trim()
            const addOnly = root.pendingAddsToLibrary
            root.pendingWallpaperPath = ""
            root.pendingAddsToLibrary = false
            wallpaperNameInput.clear()
            if (addOnly)
                root.controller.addUserWallpaper(selectedPath, displayName)
            else
                root.controller.importAndSelectWallpaper(selectedPath, displayName, root.controller.selectionFit)
        }
        onRejected: wallpaperNameInput.clear()
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
                objectName: section.showAddButton ? "userWallpapersAddButton" : ""
                visible: section.showAddButton
                enabled: !root.controller.busy
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
                    cursorShape: parent.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
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
                text: I18n.tr("apps.settings.pages.paper.wallpaper.text.no_wallpapers_found", "No wallpapers found")
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
                                text: modelData.displayName || I18n.tr("apps.settings.pages.paper.wallpaper.text.my_wallpaper", "My Wallpaper")
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
                                           root.controller.selectionFit)
                        }
                    }
                }
            }
        }
    }
}
