import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import QtCore as QtCore
import Astrea.System 1.0
import "../../components" as Components
import "../../components/typography" as Typography
import "../../components/feedback" as Feedback
import "../../components/form" as Form
import "../../components/controls" as Controls

FocusScope {
    id: root
    objectName: "devicesPage"
    readonly property var bluetooth: SettingsController.bluetooth
    property var hardwareDevices: []
    property int connectedBluetoothCount: 0
    property int knownBluetoothCount: 0
    property int lastKnownBluetoothIndex: -1
    property string pickerCategory: "all"
    readonly property bool appleIconTheme: SettingsController.navigationIconTheme === "apple"
    readonly property int selectedCategoryIndex:
        Math.max(0, iconCategories.findIndex(entry => entry.id === pickerCategory))
    property string pickerDeviceId: ""
    property string pickerDeviceName: ""
    property string pickerEffectiveKey: ""

    QtCore.Settings {
        id: iconPreferences
        category: "ConnectedDeviceIcons"
        property string overridesJson: "{}"
    }

    readonly property var iconChoices: [
        { key: "headphones", name: I18n.tr("settings.devices.headphones", "Headphones"), category: "audio" },
        { key: "speaker", name: I18n.tr("settings.devices.speaker", "Speaker"), category: "audio" },
        { key: "audio", name: I18n.tr("settings.devices.audio", "Audio"), category: "audio" },
        { key: "microphone", name: I18n.tr("settings.devices.microphone", "Microphone"), category: "audio" },
        { key: "airpods", name: I18n.tr("settings.devices.icon_airpods", "AirPods"), category: "audio" },
        { key: "airpods-pro", name: I18n.tr("settings.devices.icon_airpods_pro", "AirPods Pro"), category: "audio" },
        { key: "airpods-max", name: I18n.tr("settings.devices.icon_airpods_max", "AirPods Max"), category: "audio" },
        { key: "earbuds", name: I18n.tr("settings.devices.icon_earbuds", "Earbuds"), category: "audio" },
        { key: "headset", name: I18n.tr("settings.devices.icon_headset", "Headset"), category: "audio" },
        { key: "homepod", name: I18n.tr("settings.devices.icon_homepod", "HomePod"), category: "audio" },
        { key: "homepod-mini", name: I18n.tr("settings.devices.icon_homepod_mini", "HomePod mini"), category: "audio" },
        { key: "hifi-speaker", name: I18n.tr("settings.devices.icon_hifi_speaker", "Hi-Fi speaker"), category: "audio" },
        { key: "beats-headphones", name: I18n.tr("settings.devices.icon_beats_headphones", "Beats headphones"), category: "audio" },
        { key: "keyboard", name: I18n.tr("settings.devices.keyboard", "Keyboard"), category: "input" },
        { key: "mouse", name: I18n.tr("settings.devices.mouse", "Mouse"), category: "input" },
        { key: "gamepad", name: I18n.tr("settings.devices.controller", "Controller"), category: "input" },
        { key: "xbox", name: "Xbox", category: "input" },
        { key: "playstation", name: "PlayStation", category: "input" },
        { key: "magic-mouse", name: I18n.tr("settings.devices.icon_magic_mouse", "Magic Mouse"), category: "input" },
        { key: "apple-pencil", name: I18n.tr("settings.devices.icon_apple_pencil", "Apple Pencil"), category: "input" },
        { key: "smartphone", name: I18n.tr("settings.devices.phone", "Phone"), category: "devices" },
        { key: "tablet", name: I18n.tr("settings.devices.tablet", "Tablet"), category: "devices" },
        { key: "display", name: I18n.tr("settings.devices.computer", "Computer"), category: "devices" },
        { key: "macbook", name: I18n.tr("settings.devices.icon_macbook", "MacBook"), category: "devices" },
        { key: "mac-mini", name: I18n.tr("settings.devices.icon_mac_mini", "Mac mini"), category: "devices" },
        { key: "desktop-computer", name: I18n.tr("settings.devices.icon_desktop_computer", "Desktop PC"), category: "devices" },
        { key: "apple-watch", name: I18n.tr("settings.devices.icon_apple_watch", "Apple Watch"), category: "devices" },
        { key: "apple-tv", name: I18n.tr("settings.devices.icon_apple_tv", "Apple TV"), category: "devices" },
        { key: "vision-pro", name: I18n.tr("settings.devices.icon_vision_pro", "Vision Pro"), category: "devices" },
        { key: "tv", name: I18n.tr("settings.devices.icon_tv", "Television"), category: "devices" },
        { key: "usb", name: I18n.tr("settings.devices.usb", "USB device"), category: "storage" },
        { key: "hard-drive", name: I18n.tr("settings.devices.storage", "Storage"), category: "storage" },
        { key: "external-drive", name: I18n.tr("settings.devices.icon_external_drive", "External drive"), category: "storage" },
        { key: "internal-drive", name: I18n.tr("settings.devices.icon_internal_drive", "Internal drive"), category: "storage" },
        { key: "sd-card", name: I18n.tr("settings.devices.icon_sd_card", "SD card"), category: "storage" },
        { key: "optical-disc", name: I18n.tr("settings.devices.icon_optical_disc", "Optical disc"), category: "storage" },
        { key: "network-drive", name: I18n.tr("settings.devices.icon_network_drive", "Network drive"), category: "storage" },
        { key: "printer", name: I18n.tr("settings.devices.printer", "Printer"), category: "other" },
        { key: "camera", name: I18n.tr("settings.devices.camera", "Camera"), category: "other" },
        { key: "internet", name: I18n.tr("settings.devices.network", "Network"), category: "other" },
        { key: "bluetooth", name: I18n.tr("settings.devices.bluetooth", "Bluetooth"), category: "other" },
        { key: "wifi-router", name: I18n.tr("settings.devices.icon_wifi_router", "Wi-Fi router"), category: "other" },
        { key: "server-rack", name: I18n.tr("settings.devices.icon_server_rack", "Server rack"), category: "other" },
        { key: "scanner", name: I18n.tr("settings.devices.icon_scanner", "Scanner"), category: "other" },
        { key: "cable", name: I18n.tr("settings.devices.icon_cable", "Cable"), category: "other" },
        { key: "webcam", name: I18n.tr("settings.devices.icon_webcam", "Webcam"), category: "other" },
    ]
    // These are authentic Circle variants from the user-installed AppleOS
    // SF Symbols collection, not generated circular backgrounds.
    readonly property var circleIconChoices: [
        { key: "headphones-circle", name: I18n.tr("settings.devices.icon_headphones_circle", "Headphones circle"), category: "audio" },
        { key: "speaker-circle", name: I18n.tr("settings.devices.icon_speaker_circle", "Speaker circle"), category: "audio" },
        { key: "microphone-circle", name: I18n.tr("settings.devices.icon_microphone_circle", "Microphone circle"), category: "audio" },
        { key: "gamepad-circle", name: I18n.tr("settings.devices.icon_gamepad_circle", "Controller circle"), category: "input" },
        { key: "smartphone-circle", name: I18n.tr("settings.devices.icon_smartphone_circle", "iPhone circle"), category: "devices" },
        { key: "tv-circle", name: I18n.tr("settings.devices.icon_tv_circle", "TV circle"), category: "devices" },
        { key: "camera-circle", name: I18n.tr("settings.devices.icon_camera_circle", "Camera circle"), category: "other" },
        { key: "wifi-circle", name: I18n.tr("settings.devices.icon_wifi_circle", "Wi-Fi circle"), category: "other" },
    ]
    readonly property var allSelectableIcons: iconChoices.concat(circleIconChoices)
    readonly property var iconCategories: [
        { id: "all", title: I18n.tr("settings.devices.all", "All") },
        { id: "audio", title: I18n.tr("settings.devices.audio_group", "Audio") },
        { id: "input", title: I18n.tr("settings.devices.input_group", "Input") },
        { id: "devices", title: I18n.tr("settings.devices.devices_group", "Devices") },
        { id: "storage", title: I18n.tr("settings.devices.storage_group", "Storage") },
        { id: "other", title: I18n.tr("settings.devices.other_group", "Other") }
    ]
    // Circle variants are part of their natural categories, not a separate mode.
    // They are shown only with AppleOS because they come from authentic SF Symbols.
    readonly property var filteredIconChoices: {
        const catalog = appleIconTheme ? allSelectableIcons : iconChoices
        return pickerCategory === "all" ? catalog
            : catalog.filter(choice => choice.category === pickerCategory)
    }

    readonly property var inputDevices:
        hardwareDevices.filter(device => device.section === "input")
    readonly property var storageDevices:
        hardwareDevices.filter(device => device.section === "storage")
    readonly property var usbDevices:
        hardwareDevices.filter(device => device.section === "usb")
    readonly property int totalDevices: hardwareDevices.length + knownBluetoothCount

    DelegateModel {
        id: bluetoothCountModel
        model: root.bluetooth ? root.bluetooth.devicesModel : null
        delegate: QtObject {}
    }
    Connections {
        target: root.bluetooth ? root.bluetooth.devicesModel : null
        function onModelReset() { Qt.callLater(root.recountBluetooth) }
    }

    function recountBluetooth() {
        let connectedCount = 0
        let knownCount = 0
        let lastKnown = -1
        const items = bluetoothCountModel.items
        for (let index = 0; index < items.count; ++index) {
            const device = items.get(index).model
            if (device.connected)
                ++connectedCount
            if (device.paired || device.connected) {
                ++knownCount
                lastKnown = index
            }
        }
        root.connectedBluetoothCount = connectedCount
        root.knownBluetoothCount = knownCount
        root.lastKnownBluetoothIndex = lastKnown
        if (iconPicker.visible && root.pickerDeviceId.startsWith("bluetooth:")
                && !root.bluetoothKnown(root.pickerDeviceId))
            iconPicker.close()
    }

    function bluetoothKnown(deviceId) {
        const items = bluetoothCountModel.items
        for (let index = 0; index < items.count; ++index) {
            const device = items.get(index).model
            if ("bluetooth:" + device.objectPath === deviceId)
                return Boolean(device.paired || device.connected)
        }
        return false
    }

    function iconForBluetoothKind(kind) {
        switch (kind) {
        case System.BluetoothDeviceKind.Keyboard: return "keyboard"
        case System.BluetoothDeviceKind.Mouse: return "mouse"
        case System.BluetoothDeviceKind.Gamepad: return "gamepad"
        case System.BluetoothDeviceKind.Headphones:
        case System.BluetoothDeviceKind.Headset: return "headphones"
        case System.BluetoothDeviceKind.Speaker: return "speaker"
        case System.BluetoothDeviceKind.Audio: return "audio"
        case System.BluetoothDeviceKind.Phone: return "smartphone"
        case System.BluetoothDeviceKind.Tablet: return "tablet"
        case System.BluetoothDeviceKind.Computer: return "display"
        case System.BluetoothDeviceKind.Printer: return "printer"
        case System.BluetoothDeviceKind.Camera: return "camera"
        case System.BluetoothDeviceKind.Network: return "internet"
        default: return "bluetooth"
        }
    }

    function nameForBluetooth(name, kind) {
        const label = String(name || "").trim()
        if (label.length > 0)
            return label
        return iconChoices.find(icon => icon.key === iconForBluetoothKind(kind))?.name
            || I18n.tr("settings.devices.unnamed", "Bluetooth device")
    }

    function automaticDeviceIcon(iconKey, deviceName) {
        const name = String(deviceName || "").toLowerCase()
        if (iconKey === "gamepad") {
            if (/(dualsense|dualshock|playstation|ps4|ps5)/.test(name))
                return "playstation"
            if (/(xbox|xinput)/.test(name))
                return "xbox"
        }
        if (iconKey === "headphones" || iconKey === "audio") {
            if (/airpods.*max/.test(name)) return "airpods-max"
            if (/airpods.*pro/.test(name)) return "airpods-pro"
            if (/airpods/.test(name)) return "airpods"
            if (/beats/.test(name)) return "beats-headphones"
        }
        if (iconKey === "speaker" || iconKey === "audio") {
            if (/homepod.*mini/.test(name)) return "homepod-mini"
            if (/homepod/.test(name)) return "homepod"
        }
        if (iconKey === "display") {
            if (/macbook/.test(name)) return "macbook"
            if (/mac mini/.test(name)) return "mac-mini"
            if (/apple tv/.test(name)) return "apple-tv"
            if (/vision pro/.test(name)) return "vision-pro"
        }
        if (iconKey === "smartphone" && /apple watch/.test(name))
            return "apple-watch"
        return iconKey
    }

    // Optional AppleOS symbols remain user-local. Other themes (or a missing
    // Apple symbol) resolve to an existing Lucide category symbol instead of
    // showing a broken/missing image.
    function displayedIconKey(key) {
        const fallback = root.appleSymbolFallbacks[key]
        if (!fallback)
            return key
        if (SettingsController.navigationIconTheme === "apple") {
            const resolved = SettingsController.iconUrlForWeight(
                key, "apple", SettingsController.navigationIconWeight).toString()
            if (resolved.startsWith("file:"))
                return key
        }
        return fallback
    }

    readonly property var appleSymbolFallbacks: ({
            "headphones-circle": "headphones",
            "speaker-circle": "speaker",
            "microphone-circle": "microphone",
            "gamepad-circle": "gamepad",
            "smartphone-circle": "smartphone",
            "tv-circle": "display",
            "camera-circle": "camera",
            "wifi-circle": "internet",
            "airpods": "headphones",
            "airpods-pro": "headphones",
            "airpods-max": "headphones",
            "earbuds": "headphones",
            "headset": "headphones",
            "homepod": "speaker",
            "homepod-mini": "speaker",
            "hifi-speaker": "speaker",
            "beats-headphones": "headphones",
            "magic-mouse": "mouse",
            "apple-pencil": "mouse",
            "macbook": "display",
            "mac-mini": "display",
            "desktop-computer": "display",
            "apple-watch": "smartphone",
            "apple-tv": "display",
            "vision-pro": "display",
            "tv": "display",
            "external-drive": "hard-drive",
            "internal-drive": "hard-drive",
            "sd-card": "hard-drive",
            "optical-disc": "hard-drive",
            "network-drive": "hard-drive",
            "wifi-router": "internet",
            "server-rack": "display",
            "scanner": "printer",
            "cable": "usb",
            "webcam": "camera",
            "xbox": "gamepad",
            "playstation": "gamepad"
    })

    function customIconFor(id) {
        try {
            const choice = JSON.parse(iconPreferences.overridesJson || "{}")[String(id)]
            return typeof choice === "string"
                && allSelectableIcons.some(icon => icon.key === choice) ? choice : ""
        } catch (error) {
            return ""
        }
    }

    function changeIcon(id, key) {
        if (!id || (key !== "" && !allSelectableIcons.some(icon => icon.key === key)))
            return
        let values = {}
        try {
            const current = JSON.parse(iconPreferences.overridesJson || "{}")
            if (current && typeof current === "object" && !Array.isArray(current))
                values = current
        } catch (error) {}
        if (key.length > 0)
            values[id] = key
        else
            delete values[id]
        iconPreferences.overridesJson = JSON.stringify(values)
    }

    function refreshHardware() {
        const updated = SettingsController.connectedHardwareDevices()
        if (JSON.stringify(updated) !== JSON.stringify(hardwareDevices))
            hardwareDevices = updated
        if (iconPicker.visible && !root.pickerDeviceId.startsWith("bluetooth:")
                && !hardwareDevices.some(device => device.id === root.pickerDeviceId))
            iconPicker.close()
    }

    function openIconPicker(id, name, defaultIcon, button) {
        if (!id || !button || !iconPicker.parent)
            return
        pickerDeviceId = id
        pickerDeviceName = name
        pickerEffectiveKey = customIconFor(id) || defaultIcon
        pickerCategory = "all"
        const overlay = iconPicker.parent
        const point = button.mapToItem(overlay, 0, button.height)
        iconPicker.x = Math.max(12, Math.min(point.x - 18, overlay.width - iconPicker.width - 12))
        const bottomY = point.y + 8
        const topY = point.y - button.height - iconPicker.height - 8
        iconPicker.y = Math.max(12, Math.min(
            bottomY + iconPicker.height <= overlay.height - 12 ? bottomY : topY,
            overlay.height - iconPicker.height - 12))
        iconPicker.open()
    }

    function selectIcon(key) {
        const connected = root.pickerDeviceId.startsWith("bluetooth:")
            ? root.bluetoothKnown(root.pickerDeviceId)
            : root.hardwareDevices.some(device => device.id === root.pickerDeviceId)
        if (connected)
            changeIcon(root.pickerDeviceId, key)
        iconPicker.close()
    }

    Timer {
        interval: 4000
        repeat: true
        running: root.visible
        onTriggered: root.refreshHardware()
    }

    Component.onCompleted: {
        refreshHardware()
        recountBluetooth()
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        Form.PageHeader {
            objectName: "devicesHeading"
            title: I18n.tr("settings.devices.title", "Devices")
            subtitle: I18n.tr("settings.devices.subtitle",
                    "Connected peripherals, input devices, storage and Bluetooth accessories.")
                + " " + I18n.tr("settings.devices.summary", "%1 devices detected")
                    .replace("%1", String(root.totalDevices)) + "."

            Controls.ButtonCapsule {
                objectName: "refreshConnectedDevices"
                label: I18n.tr("settings.devices.refresh", "Refresh")
                accessibleName: label
                onClicked: root.refreshHardware()
            }
        }

        Form.SectionHeader {
            text: I18n.tr("settings.devices.input_section", "INPUT DEVICES")
            Layout.bottomMargin: Components.Theme.spacingMedium
        }
        Form.FormCard {
            Layout.bottomMargin: Components.Theme.spacingXLarge
            Repeater {
                model: root.inputDevices
                delegate: DeviceRow {
                    required property var modelData
                    required property int index
                    isLast: index === root.inputDevices.length - 1
                    deviceId: modelData.id
                    deviceName: modelData.name
                    deviceDetail: modelData.detail
                    defaultIconKey: modelData.iconKey
                }
            }
            EmptyRow {
                visible: root.inputDevices.length === 0
                text: I18n.tr("settings.devices.no_input", "No input devices detected")
            }
        }

        Form.SectionHeader {
            text: I18n.tr("settings.devices.storage_section", "STORAGE")
            Layout.bottomMargin: Components.Theme.spacingMedium
        }
        Form.FormCard {
            Layout.bottomMargin: Components.Theme.spacingXLarge
            Repeater {
                model: root.storageDevices
                delegate: DeviceRow {
                    required property var modelData
                    required property int index
                    isLast: index === root.storageDevices.length - 1
                    usage: modelData.usage !== undefined ? modelData.usage : -1
                    deviceId: modelData.id
                    deviceName: modelData.name
                    deviceDetail: modelData.detail
                    defaultIconKey: modelData.iconKey
                }
            }
            EmptyRow {
                visible: root.storageDevices.length === 0
                text: I18n.tr("settings.devices.no_storage", "No storage devices detected")
            }
        }

        Form.SectionHeader {
            text: I18n.tr("settings.devices.usb_section", "USB ACCESSORIES")
            Layout.bottomMargin: Components.Theme.spacingMedium
        }
        Form.FormCard {
            Layout.bottomMargin: Components.Theme.spacingXLarge
            Repeater {
                model: root.usbDevices
                delegate: DeviceRow {
                    required property var modelData
                    required property int index
                    isLast: index === root.usbDevices.length - 1
                    deviceId: modelData.id
                    deviceName: modelData.name
                    deviceDetail: modelData.detail
                    defaultIconKey: modelData.iconKey
                }
            }
            EmptyRow {
                visible: root.usbDevices.length === 0
                text: I18n.tr("settings.devices.no_usb", "No other USB accessories detected")
            }
        }

        Form.SectionHeader {
            text: I18n.tr("settings.devices.bluetooth_section", "BLUETOOTH")
            Layout.bottomMargin: Components.Theme.spacingMedium
        }
        Form.FormCard {
            Layout.bottomMargin: Components.Theme.spacingXLarge
            Repeater {
                model: root.bluetooth ? root.bluetooth.devicesModel : null
                delegate: DeviceRow {
                    required property string objectPath
                    required property string name
                    required property int deviceKind
                    required property bool paired
                    required property bool connected
                    required property int index
                    visible: paired || connected
                    isLast: index === root.lastKnownBluetoothIndex
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    deviceId: "bluetooth:" + objectPath
                    deviceName: root.nameForBluetooth(name, deviceKind)
                    deviceDetail: connected
                        ? I18n.tr("settings.devices.bt_connected", "Connected via Bluetooth")
                        : I18n.tr("settings.devices.bt_disconnected", "Paired · Disconnected")
                    defaultIconKey: root.iconForBluetoothKind(deviceKind)
                }
            }
            EmptyRow {
                visible: root.knownBluetoothCount === 0
                text: I18n.tr("settings.devices.no_bt", "No paired or connected Bluetooth devices")
            }
        }
    }

    component EmptyRow: Feedback.EmptyState {}

    component DeviceRow: Item {
        id: deviceRow
        property bool isLast: false
        // Fraction of the mounted space in use; negative when nothing is mounted.
        property real usage: -1
        property string deviceId: ""
        property string deviceName: ""
        property string deviceDetail: ""
        property string defaultIconKey: "usb"
        readonly property string automaticIconKey: root.automaticDeviceIcon(defaultIconKey, deviceName)
        readonly property string chosenIconKey: root.customIconFor(deviceId) || automaticIconKey
        readonly property string paintedIconKey: root.displayedIconKey(chosenIconKey)

        objectName: "deviceRow_" + deviceId.replace(/[^a-zA-Z0-9]/g, "_")
        Layout.fillWidth: true
        implicitHeight: deviceRow.usage >= 0 ? 76 : 68
        Layout.preferredHeight: visible ? implicitHeight : 0

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Components.Theme.spacingLarge
            anchors.rightMargin: Components.Theme.spacingLarge
            spacing: Components.Theme.spacingMedium

            Button {
                id: iconButton
                objectName: "deviceIconButton_" + deviceRow.deviceId.replace(/[^a-zA-Z0-9]/g, "_")
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                padding: 0
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                Accessible.name: I18n.tr("settings.devices.change_icon", "Change icon for %1")
                    .replace("%1", deviceRow.deviceName)
                Controls.HoverTooltip {
                    targetItem: iconButton
                    requested: iconButton.hovered
                    label: iconButton.Accessible.name
                }
                onClicked: root.openIconPicker(deviceRow.deviceId, deviceRow.deviceName,
                                               deviceRow.automaticIconKey, iconButton)
                background: Rectangle {
                    radius: width / 2
                    color: iconButton.down
                        ? Qt.rgba(Components.Theme.textPrimary.r, Components.Theme.textPrimary.g,
                                  Components.Theme.textPrimary.b, 0.14)
                        : iconButton.hovered || iconButton.activeFocus
                            ? Qt.rgba(Components.Theme.textPrimary.r, Components.Theme.textPrimary.g,
                                      Components.Theme.textPrimary.b, 0.09)
                            : "transparent"
                    border.width: iconButton.activeFocus ? 1 : 0
                    border.color: Qt.rgba(Components.Theme.textPrimary.r,
                                           Components.Theme.textPrimary.g,
                                           Components.Theme.textPrimary.b, 0.22)
                    Behavior on color {
                        ColorAnimation { duration: Components.Theme.animationQuick }
                    }
                }
                contentItem: Item {
                    Controls.DeviceSymbol {
                        id: deviceArtwork
                        objectName: "deviceArtwork_" + deviceRow.deviceId.replace(/[^a-zA-Z0-9]/g, "_")
                        anchors.centerIn: parent
                        width: 28
                        height: 28
                        iconKey: deviceRow.paintedIconKey
                        tint: Components.Theme.textPrimary
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3
                Typography.TextLabel {
                    Layout.fillWidth: true
                    text: deviceRow.deviceName
                    textFormat: Text.PlainText
                    textColor: Components.Theme.textPrimary
                    typographyRole: "body"
                    font.weight: Components.Theme.typography.role("body").weight
                    elide: Text.ElideRight
                }
                Typography.TextLabel {
                    Layout.fillWidth: true
                    text: deviceRow.deviceDetail
                    textFormat: Text.PlainText
                    textColor: Components.Theme.textSecondary
                    typographyRole: "caption"
                    elide: Text.ElideRight
                }
                Rectangle {
                    objectName: "deviceUsageBar_" + deviceRow.deviceId.replace(/[^a-zA-Z0-9]/g, "_")
                    visible: deviceRow.usage >= 0
                    Layout.fillWidth: true
                    Layout.maximumWidth: 260
                    Layout.topMargin: 3
                    implicitHeight: 4
                    radius: 2
                    color: Qt.rgba(Components.Theme.textTertiary.r, Components.Theme.textTertiary.g,
                                   Components.Theme.textTertiary.b, 0.3)
                    Rectangle {
                        width: parent.width * Math.max(0, Math.min(1, deviceRow.usage))
                        height: parent.height
                        radius: parent.radius
                        // Nearly full disks read as a warning, like Finder's storage bar.
                        color: deviceRow.usage > 0.9 ? Components.Theme.warningColor : Components.Theme.accent
                    }
                }
            }
        }

        Rectangle {
            visible: !deviceRow.isLast
            anchors {
                bottom: parent.bottom; right: parent.right; left: parent.left
                leftMargin: Components.Theme.spacingLarge + 42 + Components.Theme.spacingMedium
            }
            height: 1
            color: Components.Theme.cardBorder
        }
    }

    Popup {
        id: iconPicker
        objectName: "deviceIconPicker"
        parent: Overlay.overlay
        width: Math.min(468, parent ? parent.width - 24 : 468)
        height: Math.min(458, parent ? parent.height - 24 : 458)
        padding: Components.Theme.spacingLarge
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        enter: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 0; to: 1
                    duration: Components.Theme.animationNormal
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    property: "scale"
                    from: 0.92; to: 1
                    duration: Components.Theme.animationNormal
                    easing.type: Components.Theme.easingEnter
                }
            }
        }
        exit: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 1; to: 0
                    duration: Components.Theme.animationQuick
                    easing.type: Easing.InCubic
                }
                NumberAnimation {
                    property: "scale"
                    from: 1; to: 0.97
                    duration: Components.Theme.animationQuick
                    easing.type: Easing.InCubic
                }
            }
        }
        background: Rectangle {
            radius: Components.Theme.cardRadius
            // Keep the picker legible even over the transparent desktop material.
            color: Qt.rgba(Components.Theme.popupBg.r,
                           Components.Theme.popupBg.g,
                           Components.Theme.popupBg.b,
                           1.0)
            border.width: 1
            border.color: Components.Theme.cardBorder
        }
        contentItem: ColumnLayout {
            spacing: Components.Theme.spacingMedium
            RowLayout {
                Layout.fillWidth: true
                spacing: Components.Theme.spacingSmall
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Typography.TextLabel {
                        text: I18n.tr("settings.devices.picker_title", "Choose an icon")
                        textColor: Components.Theme.textPrimary
                        typographyRole: "label"
                        font.weight: Components.Theme.typography.role("label").weight
                    }
                    Typography.TextLabel {
                        text: root.pickerDeviceName
                        textColor: Components.Theme.textSecondary
                        typographyRole: "caption"
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
                Controls.ButtonCapsule {
                    objectName: "deviceIconReset"
                    label: I18n.tr("settings.devices.reset", "Default")
                    enabled: root.customIconFor(root.pickerDeviceId) !== ""
                    onClicked: root.selectIcon("")
                }
            }

            // macOS-style segmented category rail. The selection indicator
            // slides to the chosen segment rather than repainting individual pills.
            Flickable {
                id: categoryFlickable
                objectName: "deviceIconCategoriesScroll"
                Layout.fillWidth: true
                Layout.preferredHeight: 54
                Layout.topMargin: 8
                Layout.leftMargin: 5
                Layout.rightMargin: 5
                contentWidth: categoryRail.width
                contentHeight: height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.HorizontalFlick
                interactive: contentWidth > width

                Item {
                    id: categoryRail
                    width: categoryStrip.implicitWidth + 12
                    height: 36

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: Qt.rgba(Components.Theme.textPrimary.r,
                                       Components.Theme.textPrimary.g,
                                       Components.Theme.textPrimary.b, 0.055)
                        border.width: 0
                    }

                    Rectangle {
                        id: categorySelection
                        objectName: "deviceIconSelectedSegment"
                        z: 1
                        x: categoryRepeater.itemAt(root.selectedCategoryIndex)
                            ? categoryStrip.x + categoryRepeater.itemAt(root.selectedCategoryIndex).x : categoryStrip.x
                        y: 4
                        width: categoryRepeater.itemAt(root.selectedCategoryIndex)
                            ? categoryRepeater.itemAt(root.selectedCategoryIndex).width : 56
                        height: 28
                        radius: 8
                        color: Qt.rgba(Components.Theme.accent.r,
                                       Components.Theme.accent.g,
                                       Components.Theme.accent.b, 0.15)
                        border.width: 1
                        border.color: Qt.rgba(Components.Theme.accent.r,
                                              Components.Theme.accent.g,
                                              Components.Theme.accent.b, 0.72)
                        Behavior on x {
                            NumberAnimation {
                                duration: Components.Theme.animationNormal
                                easing.type: Easing.OutCubic
                            }
                        }
                        Behavior on width {
                            NumberAnimation {
                                duration: Components.Theme.animationQuick
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    Row {
                        id: categoryStrip
                        x: 6
                        y: 3
                        z: 2
                        spacing: 4
                        Repeater {
                            id: categoryRepeater
                            model: root.iconCategories
                            delegate: Button {
                                required property var modelData
                                objectName: "deviceIconCategory_" + modelData.id
                                height: 30
                                padding: 0
                                leftPadding: 14
                                rightPadding: 14
                                width: categoryLabel.implicitWidth + 28
                                focusPolicy: Qt.StrongFocus
                                hoverEnabled: true
                                Accessible.name: modelData.title
                                Accessible.description: root.pickerCategory === modelData.id
                                    ? I18n.tr("settings.devices.selected", "Selected") : ""
                                onClicked: {
                                    root.pickerCategory = modelData.id
                                    iconGrid.positionViewAtBeginning()
                                    const left = categoryStrip.x + x
                                    const right = left + width
                                    if (left < categoryFlickable.contentX)
                                        categoryFlickable.contentX = left
                                    else if (right > categoryFlickable.contentX + categoryFlickable.width)
                                        categoryFlickable.contentX = Math.min(
                                            Math.max(0, categoryFlickable.contentWidth
                                                   - categoryFlickable.width),
                                            right - categoryFlickable.width)
                                }
                                background: Rectangle {
                                    radius: 8
                                    color: parent.hovered && root.pickerCategory !== modelData.id
                                        ? Qt.rgba(Components.Theme.textPrimary.r,
                                                  Components.Theme.textPrimary.g,
                                                  Components.Theme.textPrimary.b, 0.06)
                                        : "transparent"
                                }
                                contentItem: Text {
                                    id: categoryLabel
                                    text: modelData.title
                                    color: root.pickerCategory === modelData.id
                                        ? Components.Theme.accent : Components.Theme.textSecondary
                                    font.family: Components.Theme.typography.role("caption").family
                                    font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                    lineHeight: Components.Theme.typography.role("caption").lineHeight
                                    lineHeightMode: Text.ProportionalHeight
                                    font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                    font.weight: root.pickerCategory === modelData.id
                                        ? Font.DemiBold : Font.Medium
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    Behavior on color {
                                        ColorAnimation { duration: Components.Theme.animationQuick }
                                    }
                                }
                            }
                        }
                    }
                }

                // The same Qt Quick Controls scrollbar used by ScrollPage,
                // attached to the category Flickable instead of a fake slider.
                ScrollBar.horizontal: ScrollBar {
                    id: categoryScrollBar
                    objectName: "deviceIconCategoryScrollbar"
                    // An attached ScrollBar is positioned by Flickable itself;
                    // explicit anchors introduce a horizontal binding loop.
                    policy: ScrollBar.AsNeeded
                    active: hovered || pressed || categoryFlickable.movingHorizontally
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Components.Theme.cardBorder
            }

            GridView {
                id: iconGrid
                objectName: "deviceIconGrid"
                Layout.fillWidth: true
                Layout.fillHeight: true
                // Match the viewport width after the scrollbar gutter.
                // The previous width/4 calculation lost a whole column when
                // rightMargin pushed the fourth cell outside the viewport.
                readonly property int visibleColumns: width >= 380 ? 4 : 3
                cellWidth: Math.max(1, Math.floor((width - rightMargin) / visibleColumns))
                cellHeight: 76
                model: root.filteredIconChoices
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationWraps: true
                rightMargin: 12
                ScrollBar.vertical: ScrollBar {
                    id: iconGridScrollBar
                    objectName: "deviceIconVerticalScrollbar"
                    policy: ScrollBar.AsNeeded
                }
                delegate: Button {
                    id: optionButton
                    required property var modelData
                    readonly property bool chosen: root.pickerEffectiveKey === modelData.key
                    objectName: "deviceIconOption_" + modelData.key
                    width: iconGrid.cellWidth - 4
                    height: 72
                    padding: 0
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: modelData.name
                    Controls.HoverTooltip {
                        targetItem: optionButton
                        requested: optionButton.hovered && iconPicker.visible
                        label: optionButton.modelData.name
                    }
                    Accessible.description: chosen
                        ? I18n.tr("settings.devices.selected", "Selected") : ""
                    onClicked: root.selectIcon(modelData.key)
                    background: Rectangle {
                        radius: Components.Theme.controlRadius
                        color: parent.down
                            ? Qt.rgba(Components.Theme.textPrimary.r,
                                      Components.Theme.textPrimary.g, Components.Theme.textPrimary.b, 0.16)
                            : chosen
                                ? Qt.rgba(Components.Theme.textPrimary.r,
                                          Components.Theme.textPrimary.g, Components.Theme.textPrimary.b, 0.10)
                                : parent.hovered || parent.activeFocus
                                    ? Qt.rgba(Components.Theme.textPrimary.r,
                                              Components.Theme.textPrimary.g,
                                              Components.Theme.textPrimary.b, 0.08)
                                    : "transparent"
                        border.width: chosen || parent.activeFocus ? 1 : 0
                        border.color: Qt.rgba(Components.Theme.textPrimary.r,
                                               Components.Theme.textPrimary.g,
                                               Components.Theme.textPrimary.b, chosen ? 0.18 : 0.12)
                        Behavior on color {
                            ColorAnimation { duration: Components.Theme.animationQuick }
                        }
                    }
                    contentItem: Item {
                        Column {
                            anchors.centerIn: parent
                            width: parent.width
                            spacing: 2

                            Item {
                                width: parent.width
                                height: 28
                                Controls.DeviceSymbol {
                                    id: optionArtwork
                                    objectName: "pickerSymbol_" + modelData.key
                                    anchors.centerIn: parent
                                    width: 28
                                    height: 28
                                    iconKey: root.displayedIconKey(modelData.key)
                                    tint: Components.Theme.textPrimary
                                }
                            }

                            Text {
                                width: parent.width
                                height: 28
                                text: modelData.name
                                color: chosen ? Components.Theme.textPrimary : Components.Theme.textSecondary
                                font.family: Components.Theme.typography.role("caption").family
                                font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                                lineHeight: Components.Theme.typography.role("caption").lineHeight
                                lineHeightMode: Text.ProportionalHeight
                                font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

        }
        onClosed: {
            root.pickerDeviceId = ""
            root.pickerDeviceName = ""
            root.pickerEffectiveKey = ""
        }
    }
}
