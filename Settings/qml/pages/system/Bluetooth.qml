import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models
import Astrea.System 1.0
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form
import "../../components/menu" as Menu

FocusScope {
    id: root
    objectName: "bluetoothPage"

    readonly property var bluetooth: SettingsController.bluetooth
    property bool scanLeaseHeld: false
    property bool pageActive: true
    readonly property string scanOwner: "settings-bluetooth-page"
    property int pairedDeviceCount: 0
    readonly property int otherDeviceCount:
        bluetoothDeviceCountModel.items.count - root.pairedDeviceCount

    property string menuDevicePath: ""
    property string menuDeviceName: ""
    property bool menuDeviceTrusted: false
    property string pendingForgetObjectPath: ""
    property string pendingForgetName: ""

    property var displayedRequestId: 0
    property var displayedRequestKind: null
    property string displayedDevicePath: ""
    property string displayedDeviceName: ""
    property string displayedPasskey: ""
    property int displayedEntered: -1
    property string displayedServiceUuid: ""
    property string displayedPin: ""

    DelegateModel {
        id: bluetoothDeviceCountModel
        model: root.bluetooth ? root.bluetooth.devicesModel : null
        delegate: QtObject {}
    }

    Connections {
        target: root.bluetooth ? root.bluetooth.devicesModel : null
        function onModelReset() { Qt.callLater(root.updateDeviceSectionCounts) }
    }

    function updateDeviceSectionCounts() {
        const items = bluetoothDeviceCountModel.items
        let pairedCount = 0
        for (let index = 0; index < items.count; ++index) {
            if (items.get(index).model.paired)
                ++pairedCount
        }
        root.pairedDeviceCount = pairedCount
    }

    readonly property string statusText: {
        if (!root.bluetooth || root.bluetooth.state === System.Starting)
            return I18n.tr("settings.bluetooth.status.connecting", "Connecting to Bluetooth…")
        if (root.bluetooth.state === System.Degraded)
            return I18n.tr("settings.bluetooth.status.reconnecting", "Reconnecting to Bluetooth…")
        if (!root.bluetooth.available)
            return I18n.tr("settings.bluetooth.status.unavailable", "Bluetooth service is unavailable")
        if (!root.bluetooth.adapterAvailable)
            return I18n.tr("settings.bluetooth.status.no_adapter", "No Bluetooth adapter found")
        if (root.bluetooth.powerPending)
            return I18n.tr("settings.bluetooth.status.power_pending", "Bluetooth power is changing…")
        if (!root.bluetooth.powered)
            return I18n.tr("settings.bluetooth.status.off", "Bluetooth is off")
        return I18n.tr("settings.bluetooth.status.ready", "Bluetooth is ready")
    }

    function deviceNameOrFallback(name) {
        const cleanName = String(name || "").trim()
        return cleanName.length > 0
            ? cleanName
            : I18n.tr("settings.bluetooth.device.unnamed", "Bluetooth device")
    }

    function hasDeviceName(name) {
        return String(name || "").trim().length > 0
    }

    function formatPasskey(passkey) {
        const digits = String(Number(passkey) || 0)
        return digits.padStart(6, "0")
    }

    function acquireScanLease() {
        if (root.scanLeaseHeld || !root.bluetooth)
            return
        root.scanLeaseHeld = root.bluetooth.requestScan(root.scanOwner)
    }

    function openDeviceMenu(objectPath, deviceName, trusted, x, y) {
        root.menuDevicePath = objectPath
        root.menuDeviceName = root.deviceNameOrFallback(deviceName)
        root.menuDeviceTrusted = trusted
        deviceMenu.openAt(x, y)
    }

    function askToForgetDevice(objectPath, deviceName) {
        root.pendingForgetObjectPath = objectPath
        root.pendingForgetName = root.deviceNameOrFallback(deviceName)
        deviceMenu.closeMenu()
        forgetDeviceDialog.open()
    }

    function confirmForgetDevice() {
        const objectPath = root.pendingForgetObjectPath
        root.clearForgetDevice()
        forgetDeviceDialog.close()
        if (objectPath.length > 0 && root.bluetooth)
            root.bluetooth.forgetDevice(objectPath)
    }

    function clearForgetDevice() {
        root.pendingForgetObjectPath = ""
        root.pendingForgetName = ""
    }

    function synchronizeAgentPrompt() {
        if (!root.bluetooth || !root.bluetooth.agentRequestActive) {
            if (agentDialog.visible)
                agentDialog.close()
            root.clearDisplayedAgentPrompt()
            return
        }

        const requestId = root.bluetooth.agentRequestId
        const isNewRequest = Number(root.displayedRequestId) !== Number(requestId)
        if (isNewRequest) {
            agentInput.text = ""
            root.displayedRequestId = requestId
        }

        root.displayedRequestKind = root.bluetooth.agentRequestKind
        root.displayedDevicePath = root.bluetooth.agentDevicePath
        root.displayedDeviceName = root.deviceNameOrFallback(root.bluetooth.agentDeviceName)
        root.displayedPasskey = root.formatPasskey(root.bluetooth.agentPasskey)
        root.displayedEntered = root.bluetooth.agentEntered
        root.displayedServiceUuid = root.bluetooth.agentServiceUuid
        root.displayedPin = root.bluetooth.agentDisplayPin

        if (!agentDialog.visible)
            agentDialog.open()
        if (isNewRequest)
            Qt.callLater(root.focusCurrentAgentPrompt)
    }

    function clearDisplayedAgentPrompt() {
        root.displayedRequestId = 0
        root.displayedRequestKind = null
        root.displayedDevicePath = ""
        root.displayedDeviceName = ""
        root.displayedPasskey = ""
        root.displayedEntered = -1
        root.displayedServiceUuid = ""
        root.displayedPin = ""
        agentInput.text = ""
    }

    function focusCurrentAgentPrompt() {
        if (agentInput.visible)
            agentInput.forceActiveFocus()
        else if (agentRejectButton.visible)
            agentRejectButton.forceActiveFocus()
        else
            agentCancelPairingButton.forceActiveFocus()
    }

    function restorePageFocus() {
        if (!root.pageActive)
            return
        if (root.bluetooth && root.bluetooth.agentRequestActive)
            return
        if (bluetoothPowerToggle.enabled)
            bluetoothPowerToggle.forceActiveFocus()
        else
            root.forceActiveFocus()
    }

    function displayedAgentTextIsValid() {
        if (root.displayedRequestKind === System.PinCodeInput)
            return agentInput.text.length > 0 && agentInput.text.length <= 16
        if (root.displayedRequestKind === System.PasskeyInput)
            return /^[0-9]{1,6}$/.test(agentInput.text)
        return false
    }

    function submitDisplayedAgentText() {
        if (!root.bluetooth || !root.displayedAgentTextIsValid())
            return
        const capturedRequestId = root.displayedRequestId
        if (!root.bluetooth.submitAgentText(capturedRequestId, agentInput.text))
            root.synchronizeAgentPrompt()
    }

    function rejectDisplayedAgentRequest() {
        if (!root.bluetooth || !agentDialog.visible)
            return
        const capturedRequestId = root.displayedRequestId
        if (root.displayedRequestKind === System.PasskeyConfirmation
                || root.displayedRequestKind === System.Authorization
                || root.displayedRequestKind === System.ServiceAuthorization) {
            if (!root.bluetooth.confirmAgentRequest(capturedRequestId, false))
                root.synchronizeAgentPrompt()
            return
        }
        if (!root.bluetooth.rejectAgentRequest(capturedRequestId))
            root.synchronizeAgentPrompt()
    }

    function confirmDisplayedAgentRequest() {
        if (!root.bluetooth || !agentDialog.visible)
            return
        const capturedRequestId = root.displayedRequestId
        if (!root.bluetooth.confirmAgentRequest(capturedRequestId, true))
            root.synchronizeAgentPrompt()
    }

    function cancelAgentPairing() {
        if (root.bluetooth && root.bluetooth.pairing)
            root.bluetooth.cancelPairing()
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 4
            spacing: Components.Theme.spacingLarge

            Text {
                Layout.fillWidth: true
                text: I18n.tr("settings.bluetooth.title", "Bluetooth")
                color: Components.Theme.textPrimary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeHero
                font.weight: Components.Theme.fontWeightDemiBold
                Accessible.name: text
            }

            Controls.ToggleSwitch {
                id: bluetoothPowerToggle
                objectName: "bluetoothPowerToggle"
                checked: root.bluetooth ? root.bluetooth.powered : false
                enabled: root.bluetooth
                    && root.bluetooth.available
                    && root.bluetooth.adapterAvailable
                    && !root.bluetooth.powerPending
                    && !root.bluetooth.pairing
                accessibleName: I18n.tr("settings.bluetooth.accessibility.power", "Bluetooth power")
                Accessible.checked: checked
                onToggled: targetChecked => root.bluetooth.setPowered(targetChecked)
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.bottomMargin: Components.Theme.spacingXLarge
            text: I18n.tr("settings.bluetooth.accessories_help",
                          "Connect to accessories you can use for music streaming, typing, and gaming.")
            color: Components.Theme.textSecondary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeNormal
            wrapMode: Text.WordWrap
        }

        Form.FormCard {
            Layout.bottomMargin: Components.Theme.spacingXLarge
            margins: Components.Theme.spacingLarge

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Components.Theme.spacingSmall

                Text {
                    objectName: "bluetoothServiceStatus"
                    Layout.fillWidth: true
                    text: root.statusText
                    color: root.bluetooth && root.bluetooth.available
                           ? Components.Theme.textPrimary
                           : Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeLarge
                    font.weight: Components.Theme.fontWeightMedium
                    wrapMode: Text.WordWrap
                }

                Text {
                    objectName: "bluetoothServiceError"
                    visible: root.bluetooth && root.bluetooth.errorString.length > 0
                        && (!root.bluetooth.available || root.bluetooth.state === System.Degraded)
                    Layout.fillWidth: true
                    text: root.bluetooth ? root.bluetooth.errorString : ""
                    color: Components.Theme.errorColor
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                Text {
                    objectName: "bluetoothPairingError"
                    visible: root.bluetooth && root.bluetooth.pairingError.length > 0
                    Layout.fillWidth: true
                    text: root.bluetooth ? root.bluetooth.pairingError : ""
                    color: Components.Theme.warningColor
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                Text {
                    objectName: "bluetoothOperationError"
                    visible: root.bluetooth && root.bluetooth.operationError.length > 0
                    Layout.fillWidth: true
                    text: root.bluetooth ? root.bluetooth.operationError : ""
                    color: Components.Theme.warningColor
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.bluetooth && root.bluetooth.pairing

                    Text {
                        Layout.fillWidth: true
                        text: I18n.tr("settings.bluetooth.pairing.active", "Pairing with %1")
                            .replace("%1", root.deviceNameOrFallback(root.bluetooth
                                ? root.bluetooth.pairingDeviceName : ""))
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeNormal
                        wrapMode: Text.WordWrap
                    }

                    Controls.ButtonCapsule {
                        id: cancelPairingButton
                        objectName: "cancelPairingButton"
                        label: I18n.tr("settings.bluetooth.action.cancel_pairing", "Cancel pairing")
                        accessibleName: label
                        onClicked: root.bluetooth.cancelPairing()
                    }
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.connectivity.bluetooth.text.meus_dispositivos",
                          "MY DEVICES")
            Layout.bottomMargin: Components.Theme.spacingMedium
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: Components.Theme.spacingXLarge
            spacing: Components.Theme.spacingSmall

            Repeater {
                id: pairedDeviceRepeater
                model: root.bluetooth ? root.bluetooth.devicesModel : null
                delegate: BluetoothDeviceRow {
                    pairedSection: true
                }
            }

            Text {
                objectName: "myDevicesEmptyState"
                visible: root.pairedDeviceCount === 0
                Layout.fillWidth: true
                text: I18n.tr("settings.bluetooth.empty.my_devices", "No paired devices yet")
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeNormal
            }
        }

        Form.SectionHeader {
            text: I18n.tr("apps.settings.pages.connectivity.bluetooth.text.outros_dispositivos",
                          "OTHER DEVICES")
            Layout.bottomMargin: Components.Theme.spacingMedium
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: Components.Theme.spacingXLarge
            spacing: Components.Theme.spacingSmall

            Repeater {
                id: otherDeviceRepeater
                model: root.bluetooth ? root.bluetooth.devicesModel : null
                delegate: BluetoothDeviceRow {
                    pairedSection: false
                }
            }

            Text {
                objectName: "otherDevicesEmptyState"
                visible: root.otherDeviceCount === 0
                Layout.fillWidth: true
                text: I18n.tr("settings.bluetooth.empty.other_devices", "No other devices found")
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeNormal
            }
        }
    }

    component BluetoothDeviceRow: Form.FormCard {
            id: deviceRow
            property bool pairedSection: true
            required property string objectPath
            required property string name
            required property bool paired
            required property bool trusted
            required property bool connected
            required property bool discovered
            required property string icon
            required property int rssi
            required property int batteryPercent

            readonly property string deviceId: {
                const pathParts = objectPath.split("/")
                return pathParts.length > 0 && pathParts[pathParts.length - 1].length > 0
                    ? pathParts[pathParts.length - 1] : objectPath
            }
            readonly property string sectionObjectSuffix: pairedSection ? "mine" : "other"
            readonly property string displayName: root.deviceNameOrFallback(name)
            readonly property bool activePairingDevice: root.bluetooth && root.bluetooth.pairing
                && root.bluetooth.pairingDevicePath === objectPath
            readonly property bool belongsInSection: paired
                ? pairedSection : (!pairedSection && discovered)

            objectName: "deviceRow_" + deviceId + "_" + sectionObjectSuffix
            visible: belongsInSection
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            margins: Components.Theme.spacingMedium
            spacing: Components.Theme.spacingSmall

            RowLayout {
                Layout.fillWidth: true
                spacing: Components.Theme.spacingMedium

                Text {
                    Layout.preferredWidth: 36
                    horizontalAlignment: Text.AlignHCenter
                    text: {
                        const iconName = String(deviceRow.icon || "").toLowerCase()
                        if (iconName.indexOf("keyboard") >= 0)
                            return "\uf11c"
                        if (iconName.indexOf("gamepad") >= 0 || iconName.indexOf("joystick") >= 0)
                            return "\uf11b"
                        if (iconName.indexOf("headset") >= 0 || iconName.indexOf("headphone") >= 0
                                || iconName.indexOf("audio") >= 0)
                            return "\uf025"
                        return "\uf293"
                    }
                    color: deviceRow.connected ? Components.Theme.accent : Components.Theme.textSecondary
                    font.family: "JetBrainsMono Nerd Font"
                    font.pixelSize: Components.Theme.fontSizeIconLarge
                    Accessible.name: I18n.tr("settings.bluetooth.accessibility.device_icon",
                                              "Bluetooth device icon")
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        objectName: "deviceName_" + deviceRow.deviceId + "_" + deviceRow.sectionObjectSuffix
                        Layout.fillWidth: true
                        text: deviceRow.displayName
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeNormal
                        font.weight: Components.Theme.fontWeightMedium
                        elide: Text.ElideRight
                        Accessible.name: deviceRow.displayName
                    }

                    Text {
                        objectName: "deviceMeta_" + deviceRow.deviceId + "_" + deviceRow.sectionObjectSuffix
                        Layout.fillWidth: true
                        text: deviceRow.activePairingDevice
                            ? I18n.tr("settings.bluetooth.status.pairing", "Pairing")
                            : deviceRow.connected
                                ? I18n.tr("settings.bluetooth.status.connected", "Connected")
                                : deviceRow.paired
                                    ? I18n.tr("settings.bluetooth.status.paired", "Paired")
                                    : I18n.tr("settings.bluetooth.status.available", "Available to pair")
                        color: deviceRow.activePairingDevice
                            ? Components.Theme.warningColor : Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }

                    Text {
                        objectName: "deviceDetails_" + deviceRow.deviceId + "_" + deviceRow.sectionObjectSuffix
                        Layout.fillWidth: true
                        visible: deviceRow.batteryPercent >= 0 || deviceRow.rssi !== -1
                        text: {
                            const battery = deviceRow.batteryPercent >= 0
                                ? I18n.tr("settings.bluetooth.device.battery", "%1% battery")
                                      .replace("%1", String(deviceRow.batteryPercent))
                                : ""
                            const signal = deviceRow.rssi !== -1
                                ? I18n.tr("settings.bluetooth.device.signal", "%1 dBm signal")
                                      .replace("%1", String(deviceRow.rssi))
                                : ""
                            return battery.length > 0 && signal.length > 0
                                ? battery + " · " + signal : battery + signal
                        }
                        color: Components.Theme.textTertiary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeTiny
                    }
                }

                Controls.ButtonCapsule {
                    objectName: "deviceAction_" + deviceRow.deviceId + "_" + deviceRow.sectionObjectSuffix
                    Layout.alignment: Qt.AlignVCenter
                    label: deviceRow.activePairingDevice
                        ? I18n.tr("settings.bluetooth.status.pairing", "Pairing")
                        : deviceRow.paired
                            ? deviceRow.connected
                                ? I18n.tr("settings.bluetooth.action.disconnect", "Disconnect")
                                : I18n.tr("settings.bluetooth.action.connect", "Connect")
                            : I18n.tr("settings.bluetooth.action.pair", "Pair")
                    accessibleName: label + " " + deviceRow.displayName
                    enabled: Boolean(root.bluetooth && root.bluetooth.available
                        && root.bluetooth.adapterAvailable && root.bluetooth.powered)
                        && (deviceRow.paired || !root.bluetooth.pairing)
                        && !deviceRow.activePairingDevice
                    onClicked: {
                        if (deviceRow.paired) {
                            if (deviceRow.connected)
                                root.bluetooth.disconnectDevice(deviceRow.objectPath)
                            else
                                root.bluetooth.connectDevice(deviceRow.objectPath)
                        } else {
                            root.bluetooth.pairDevice(deviceRow.objectPath)
                        }
                    }
                }

                Controls.ButtonCapsule {
                    objectName: "deviceMore_" + deviceRow.deviceId + "_" + deviceRow.sectionObjectSuffix
                    visible: deviceRow.paired
                    label: I18n.tr("settings.bluetooth.action.more", "More")
                    accessibleName: I18n.tr("settings.bluetooth.accessibility.device_actions",
                                             "Actions for %1").replace("%1", deviceRow.displayName)
                    onClicked: {
                        const point = mapToItem(root, width, height / 2)
                        root.openDeviceMenu(deviceRow.objectPath, deviceRow.displayName,
                                            deviceRow.trusted, point.x, point.y)
                    }
                }
            }
        }

    Menu.ContextMenu {
        id: deviceMenu
        objectName: "deviceContextMenu"
        panelColor: Components.Theme.popupBg
        borderColor: Components.Theme.cardBorder

        Menu.ContextMenuAction {
            objectName: "trustDeviceAction"
            label: root.menuDeviceTrusted
                ? I18n.tr("settings.bluetooth.action.untrust", "Untrust")
                : I18n.tr("settings.bluetooth.action.trust", "Trust")
            onTriggered: {
                root.bluetooth.setDeviceTrusted(root.menuDevicePath, !root.menuDeviceTrusted)
                deviceMenu.closeMenu()
            }
        }

        Menu.ContextMenuDivider { lineColor: Components.Theme.cardBorder }

        Menu.ContextMenuAction {
            objectName: "forgetDeviceAction"
            label: I18n.tr("settings.bluetooth.action.forget_device", "Forget Device")
            destructive: true
            hoverColor: Components.Theme.windowWash
            textColor: Components.Theme.errorColor
            onTriggered: root.askToForgetDevice(root.menuDevicePath, root.menuDeviceName)
        }
    }

    Dialog {
        id: forgetDeviceDialog
        objectName: "forgetDeviceDialog"
        modal: true
        width: 380
        padding: Components.Theme.spacingXLarge
        closePolicy: Popup.CloseOnEscape
        anchors.centerIn: Overlay.overlay
        Overlay.modal: Rectangle { color: Components.Theme.windowWash }

        background: Rectangle {
            radius: Components.Theme.cardRadius
            color: Components.Theme.cardBg
            border.width: 1
            border.color: Components.Theme.cardBorder
        }

        contentItem: ColumnLayout {
            spacing: Components.Theme.spacingMedium

            Text {
                objectName: "forgetDevicePrompt"
                Layout.fillWidth: true
                text: I18n.tr("settings.bluetooth.forget.title", "Forget %1?")
                    .replace("%1", root.pendingForgetName)
                color: Components.Theme.textPrimary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeLarge
                font.weight: Components.Theme.fontWeightMedium
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: I18n.tr("settings.bluetooth.forget.help",
                              "This removes the device from your Bluetooth devices.")
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeNormal
                wrapMode: Text.WordWrap
            }
        }

        footer: RowLayout {
            spacing: Components.Theme.spacingSmall

            Controls.ButtonCapsule {
                objectName: "forgetDeviceCancelButton"
                Layout.fillWidth: true
                label: I18n.tr("settings.bluetooth.action.cancel", "Cancel")
                onClicked: forgetDeviceDialog.reject()
            }

            Controls.ButtonCapsule {
                objectName: "forgetDeviceConfirmButton"
                Layout.fillWidth: true
                label: I18n.tr("settings.bluetooth.action.forget", "Forget")
                primary: true
                danger: true
                enabled: root.pendingForgetObjectPath.length > 0
                onClicked: root.confirmForgetDevice()
            }
        }

        onOpened: forgetDeviceCancelButton.forceActiveFocus()
        onRejected: root.clearForgetDevice()
        onClosed: {
            root.clearForgetDevice()
            root.restorePageFocus()
        }
    }

    Dialog {
        id: agentDialog
        objectName: "bluetoothAgentDialog"
        modal: true
        width: 420
        padding: Components.Theme.spacingXLarge
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        Overlay.modal: Rectangle { color: Components.Theme.windowWash }

        background: Rectangle {
            radius: Components.Theme.cardRadius
            color: Components.Theme.cardBg
            border.width: 1
            border.color: Components.Theme.cardBorder
        }

        contentItem: ColumnLayout {
            spacing: Components.Theme.spacingMedium

            Text {
                Layout.fillWidth: true
                text: I18n.tr("settings.bluetooth.agent.title", "Bluetooth request")
                color: Components.Theme.textPrimary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeLarge
                font.weight: Components.Theme.fontWeightMedium
            }

            Text {
                id: agentPromptText
                objectName: "agentPromptText"
                Layout.fillWidth: true
                text: {
                    if (root.displayedRequestKind === System.PinCodeInput)
                        return I18n.tr("settings.bluetooth.agent.pin_prompt",
                                        "Enter the PIN for \"%1\"").replace("%1", root.displayedDeviceName)
                    if (root.displayedRequestKind === System.PasskeyInput)
                        return I18n.tr("settings.bluetooth.agent.passkey_prompt",
                                        "Enter the passkey for \"%1\"").replace("%1", root.displayedDeviceName)
                    if (root.displayedRequestKind === System.PasskeyConfirmation)
                        return root.displayedPasskey
                    if (root.displayedRequestKind === System.Authorization)
                        return I18n.tr("settings.bluetooth.agent.authorization_prompt",
                                        "Allow \"%1\" to pair?").replace("%1", root.displayedDeviceName)
                    if (root.displayedRequestKind === System.ServiceAuthorization)
                        return I18n.tr("settings.bluetooth.agent.service_authorization_prompt",
                                        "Allow \"%1\" to use service %2?")
                            .replace("%1", root.displayedDeviceName)
                            .replace("%2", root.displayedServiceUuid)
                    if (root.displayedRequestKind === System.DisplayPinCode)
                        return root.displayedPin
                    if (root.displayedRequestKind === System.DisplayPasskey)
                        return root.displayedPasskey
                    return ""
                }
                color: Components.Theme.textPrimary
                font.family: root.displayedRequestKind === System.PasskeyConfirmation
                    || root.displayedRequestKind === System.DisplayPinCode
                    || root.displayedRequestKind === System.DisplayPasskey
                    ? Components.Theme.monoFontFamily : Components.Theme.fontFamily
                font.pixelSize: root.displayedRequestKind === System.PasskeyConfirmation
                    || root.displayedRequestKind === System.DisplayPinCode
                    || root.displayedRequestKind === System.DisplayPasskey
                    ? Components.Theme.fontSizeHero : Components.Theme.fontSizeNormal
                font.weight: Components.Theme.fontWeightMedium
                horizontalAlignment: root.displayedRequestKind === System.PasskeyConfirmation
                    || root.displayedRequestKind === System.DisplayPinCode
                    || root.displayedRequestKind === System.DisplayPasskey
                    ? Text.AlignHCenter : Text.AlignLeft
                wrapMode: Text.WordWrap
                Accessible.name: text
            }

            Text {
                visible: root.displayedRequestKind === System.DisplayPinCode
                    || root.displayedRequestKind === System.DisplayPasskey
                Layout.fillWidth: true
                text: root.displayedRequestKind === System.DisplayPinCode
                    ? I18n.tr("settings.bluetooth.agent.display_pin_help",
                              "Enter this PIN on the other device.")
                    : I18n.tr("settings.bluetooth.agent.display_passkey_help",
                              "Enter this passkey on the other device.")
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeNormal
                wrapMode: Text.WordWrap
            }

            Text {
                id: agentProgressText
                objectName: "agentProgressText"
                visible: root.displayedRequestKind === System.DisplayPasskey
                Layout.fillWidth: true
                text: I18n.tr("settings.bluetooth.agent.passkey_progress",
                              "%1 of 6 digits entered").replace("%1", String(root.displayedEntered))
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
                horizontalAlignment: Text.AlignHCenter
            }

            TextField {
                id: agentInput
                objectName: "agentTextInput"
                visible: root.displayedRequestKind === System.PinCodeInput
                    || root.displayedRequestKind === System.PasskeyInput
                Layout.fillWidth: true
                maximumLength: 64
                placeholderText: root.displayedRequestKind === System.PinCodeInput
                    ? I18n.tr("settings.bluetooth.agent.pin_placeholder", "PIN (up to 16 characters)")
                    : I18n.tr("settings.bluetooth.agent.passkey_placeholder", "Passkey (up to 6 digits)")
                inputMethodHints: root.displayedRequestKind === System.PasskeyInput
                    ? Qt.ImhDigitsOnly : Qt.ImhNone
                selectByMouse: true
                Accessible.name: root.displayedRequestKind === System.PinCodeInput
                    ? I18n.tr("settings.bluetooth.agent.pin_accessibility", "Bluetooth PIN")
                    : I18n.tr("settings.bluetooth.agent.passkey_accessibility", "Bluetooth passkey")
                background: Rectangle {
                    radius: Components.Theme.controlRadius
                    color: Components.Theme.popupBg
                    border.width: 1
                    border.color: agentInput.activeFocus
                        ? Components.Theme.accent : Components.Theme.cardBorder
                }
                Keys.onReturnPressed: if (root.displayedAgentTextIsValid()) root.submitDisplayedAgentText()
                Keys.onEnterPressed: if (root.displayedAgentTextIsValid()) root.submitDisplayedAgentText()
            }
        }

        footer: RowLayout {
            spacing: Components.Theme.spacingSmall

            Controls.ButtonCapsule {
                id: agentRejectButton
                objectName: "agentRejectButton"
                visible: root.displayedRequestKind === System.PinCodeInput
                    || root.displayedRequestKind === System.PasskeyInput
                    || root.displayedRequestKind === System.PasskeyConfirmation
                    || root.displayedRequestKind === System.Authorization
                    || root.displayedRequestKind === System.ServiceAuthorization
                Layout.fillWidth: true
                label: root.displayedRequestKind === System.PasskeyConfirmation
                    ? I18n.tr("settings.bluetooth.action.cancel", "Cancel")
                    : root.displayedRequestKind === System.Authorization
                        || root.displayedRequestKind === System.ServiceAuthorization
                    ? I18n.tr("settings.bluetooth.action.reject", "Reject")
                    : I18n.tr("settings.bluetooth.action.cancel", "Cancel")
                onClicked: root.rejectDisplayedAgentRequest()
            }

            Controls.ButtonCapsule {
                id: agentAllowButton
                objectName: "agentAllowButton"
                visible: root.displayedRequestKind === System.Authorization
                    || root.displayedRequestKind === System.ServiceAuthorization
                Layout.fillWidth: true
                primary: true
                label: I18n.tr("settings.bluetooth.action.allow", "Allow")
                onClicked: root.confirmDisplayedAgentRequest()
            }

            Controls.ButtonCapsule {
                id: agentSubmitButton
                objectName: "agentSubmitButton"
                visible: root.displayedRequestKind === System.PinCodeInput
                    || root.displayedRequestKind === System.PasskeyInput
                Layout.fillWidth: true
                primary: true
                enabled: root.displayedAgentTextIsValid()
                label: I18n.tr("settings.bluetooth.action.submit", "Submit")
                onClicked: root.submitDisplayedAgentText()
            }

            Controls.ButtonCapsule {
                id: agentConfirmButton
                objectName: "agentConfirmButton"
                visible: root.displayedRequestKind === System.PasskeyConfirmation
                Layout.fillWidth: true
                primary: true
                label: I18n.tr("settings.bluetooth.action.confirm", "Confirm")
                onClicked: root.confirmDisplayedAgentRequest()
            }

            Controls.ButtonCapsule {
                id: agentCancelPairingButton
                objectName: "agentCancelPairingButton"
                visible: root.displayedRequestKind === System.DisplayPinCode
                    || root.displayedRequestKind === System.DisplayPasskey
                Layout.fillWidth: true
                danger: true
                label: I18n.tr("settings.bluetooth.action.cancel_pairing", "Cancel pairing")
                onClicked: root.cancelAgentPairing()
            }
        }

        onOpened: root.focusCurrentAgentPrompt()
        onClosed: Qt.callLater(root.restorePageFocus)
    }

    Shortcut {
        sequence: "Escape"
        enabled: agentDialog.visible
        context: Qt.ApplicationShortcut
        onActivated: {
            if (root.displayedRequestKind === System.DisplayPinCode
                    || root.displayedRequestKind === System.DisplayPasskey)
                root.cancelAgentPairing()
            else
                root.rejectDisplayedAgentRequest()
        }
    }

    Connections {
        target: root.bluetooth
        function onAgentRequestChanged() { root.synchronizeAgentPrompt() }
    }

    Component.onCompleted: {
        root.updateDeviceSectionCounts()
        root.acquireScanLease()
        root.synchronizeAgentPrompt()
    }

    Component.onDestruction: {
        root.pageActive = false
        if (root.scanLeaseHeld && root.bluetooth) {
            root.bluetooth.releaseScan(root.scanOwner)
            root.scanLeaseHeld = false
        }
        if (root.bluetooth && root.bluetooth.pairing)
            root.bluetooth.cancelPairing()
    }
}
