import QtQuick
import QtQuick.Layouts
import Astrea.System 1.0
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

FocusScope {
    id: root
    objectName: "internetPage"

    readonly property var network:
        typeof SettingsController !== "undefined" && SettingsController
            ? SettingsController.network : null
    readonly property int serviceState: root.network ? root.network.state : System.Unavailable
    readonly property bool serviceReady:
        root.network !== null && root.serviceState === System.Ready
    readonly property bool isConnected:
        root.serviceReady && root.network.connected
            && root.network.connectionType !== System.NetworkConnectionType.None
    readonly property bool showConnectionDetails: root.serviceReady && root.isConnected
    readonly property bool isWiredConnection: root.showConnectionDetails
        && root.network.connectionType === System.NetworkConnectionType.Wired
    readonly property bool hasConnectionDetailValue: root.network
        && (root.network.ipv4Addresses.length > 0 || root.network.ipv4Gateway !== ""
            || root.network.ipv6Addresses.length > 0 || root.network.ipv6Gateway !== ""
            || root.network.hardwareAddress !== "" || root.network.mtu > 0
            || root.isWiredConnection)
    readonly property bool canSetWifi:
        root.serviceReady && root.network.available && root.network.wifiAvailable
            && root.network.wifiHardwareEnabled && !root.network.wifiPending
    readonly property bool canScanWifi:
        root.serviceReady && root.network.available && root.network.wifiAvailable
            && root.network.wifiHardwareEnabled && root.network.wifiEnabled
            && !root.network.wifiScanning && !root.network.wifiPending

    function serviceStatusText() {
        switch (root.serviceState) {
        case System.Starting:
            return I18n.tr("settings.internet.service.starting", "Connecting to NetworkManager…")
        case System.Ready:
            return I18n.tr("settings.internet.service.ready", "Network service is ready")
        case System.Unavailable:
            return I18n.tr("settings.internet.service.unavailable", "NetworkManager is unavailable")
        case System.Degraded:
            return I18n.tr("settings.internet.service.degraded", "Network service is reconnecting")
        case System.Stopped:
        default:
            return I18n.tr("settings.internet.service.stopped", "Network service is inactive")
        }
    }

    function connectionTypeText() {
        if (!root.isConnected)
            return I18n.tr("settings.internet.connection.disconnected", "Disconnected")
        switch (root.network.connectionType) {
        case System.NetworkConnectionType.Wifi:
            return I18n.tr("settings.internet.connection.wifi", "Wi-Fi")
        case System.NetworkConnectionType.Wired:
            return I18n.tr("settings.internet.connection.ethernet", "Ethernet")
        case System.NetworkConnectionType.Other:
            return I18n.tr("settings.internet.connection.other", "Network")
        case System.NetworkConnectionType.None:
        default:
            return I18n.tr("settings.internet.connection.disconnected", "Disconnected")
        }
    }

    function frequencyBandText(frequencyMHz) {
        if (frequencyMHz <= 0)
            return ""
        if (frequencyMHz < 3000)
            return I18n.tr("settings.internet.network.band_2_4", "2.4 GHz")
        if (frequencyMHz < 5925)
            return I18n.tr("settings.internet.network.band_5", "5 GHz")
        return I18n.tr("settings.internet.network.band_6", "6 GHz")
    }

    function linkText() {
        return root.network && root.network.linkCarrier
            ? I18n.tr("settings.internet.details.link_up", "Up")
            : I18n.tr("settings.internet.details.link_down", "Down")
    }

    function linkSpeedText(speedMbps) {
        if (speedMbps <= 0)
            return ""
        if (speedMbps < 1000) {
            return I18n.tr("settings.internet.details.speed_mbps", "{speed} Mbps")
                .replace("{speed}", String(speedMbps))
        }
        const gigabits = speedMbps / 1000
        const value = Number.isInteger(gigabits) ? String(gigabits)
                                                 : String(Number(gigabits.toFixed(1)))
        return I18n.tr("settings.internet.details.speed_gbps", "{speed} Gbps")
            .replace("{speed}", value)
    }

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 32
        maxWidth: 900

        ColumnLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: Components.Theme.spacingXLarge
            spacing: Components.Theme.spacingTiny

            Text {
                objectName: "internetHeading"
                Layout.fillWidth: true
                text: I18n.tr("settings.internet.title", "Internet")
                color: Components.Theme.textPrimary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeHeader
                font.weight: Components.Theme.fontWeightBold
            }

            Text {
                Layout.fillWidth: true
                text: I18n.tr("settings.internet.subtitle",
                              "Manage your network connection and nearby Wi-Fi networks.")
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSubtitle
                wrapMode: Text.WordWrap
            }
        }

        Form.SectionHeader {
            text: I18n.tr("settings.internet.section.network_status", "NETWORK STATUS")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 24
            margins: Components.Theme.spacingLarge

            ColumnLayout {
                width: parent.width
                spacing: Components.Theme.spacingSmall

                Text {
                    objectName: "networkServiceStatus"
                    Layout.fillWidth: true
                    text: root.serviceStatusText()
                    color: root.serviceReady
                        ? Components.Theme.textPrimary : Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeLarge
                    font.weight: Components.Theme.fontWeightMedium
                    wrapMode: Text.WordWrap
                }

                Text {
                    objectName: "networkServiceError"
                    Layout.fillWidth: true
                    visible: root.network
                        && (root.serviceState === System.Unavailable
                            || root.serviceState === System.Degraded)
                        && root.network.errorString !== ""
                    text: root.network ? root.network.errorString : ""
                    textFormat: Text.PlainText
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }

                Text {
                    objectName: "networkConnectionState"
                    Layout.fillWidth: true
                    text: root.isConnected
                        ? I18n.tr("settings.internet.connection.connected", "Network connected")
                        : I18n.tr("settings.internet.connection.disconnected", "Disconnected")
                    color: root.isConnected
                        ? Components.Theme.textPrimary : Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeNormal
                    font.weight: Components.Theme.fontWeightMedium
                }

                ColumnLayout {
                    objectName: "connectionDetails"
                    Layout.fillWidth: true
                    visible: root.isConnected
                    spacing: Components.Theme.spacingMicro

                    Text {
                        objectName: "connectionName"
                        Layout.fillWidth: true
                        text: root.isConnected ? root.network.connectionName : ""
                        textFormat: Text.PlainText
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeLarge
                        font.weight: Components.Theme.fontWeightMedium
                        elide: Text.ElideRight
                    }

                    Text {
                        objectName: "connectionTypeLabel"
                        Layout.fillWidth: true
                        text: root.connectionTypeText()
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }

                    Text {
                        objectName: "connectionInterface"
                        Layout.fillWidth: true
                        visible: root.isConnected && root.network.interfaceName !== ""
                        text: visible
                            ? I18n.tr("settings.internet.connection.interface",
                                      "Interface: {name}").replace("{name}", root.network.interfaceName)
                            : ""
                        textFormat: Text.PlainText
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.isConnected
                        spacing: Components.Theme.spacingLarge

                        Text {
                            objectName: "downloadRate"
                            text: I18n.tr("settings.internet.connection.download",
                                          "Download: {rate}").replace("{rate}", root.network.downloadRate)
                            textFormat: Text.PlainText
                            color: Components.Theme.textSecondary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeSmall
                        }

                        Text {
                            objectName: "uploadRate"
                            text: I18n.tr("settings.internet.connection.upload",
                                          "Upload: {rate}").replace("{rate}", root.network.uploadRate)
                            textFormat: Text.PlainText
                            color: Components.Theme.textSecondary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeSmall
                        }
                    }
                }
            }
        }

        Form.SectionHeader {
            objectName: "connectionDetailsSection"
            visible: root.showConnectionDetails
            text: I18n.tr("settings.internet.section.connection_details", "CONNECTION DETAILS")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "connectionDetailsCard"
            visible: root.showConnectionDetails
            Layout.bottomMargin: 24

            ColumnLayout {
                width: parent.width
                spacing: 0

                Form.SettingRow {
                    objectName: "connectionIpv4Addresses"
                    visible: root.network && root.network.ipv4Addresses.length > 0
                    label: I18n.tr("settings.internet.details.ipv4_address", "IPv4 address")
                    sublabel: visible ? root.network.ipv4Addresses.join("\n") : ""
                    sublabelTextFormat: Text.PlainText
                    isLast: !root.network || (root.network.ipv4Gateway === ""
                        && root.network.ipv6Addresses.length === 0
                        && root.network.ipv6Gateway === ""
                        && root.network.hardwareAddress === "" && root.network.mtu <= 0
                        && !root.isWiredConnection)
                }

                Form.SettingRow {
                    objectName: "connectionIpv4Gateway"
                    visible: root.network && root.network.ipv4Gateway !== ""
                    label: I18n.tr("settings.internet.details.ipv4_gateway", "IPv4 gateway")
                    sublabel: visible ? root.network.ipv4Gateway : ""
                    sublabelTextFormat: Text.PlainText
                    isLast: !root.network || (root.network.ipv6Addresses.length === 0
                        && root.network.ipv6Gateway === ""
                        && root.network.hardwareAddress === "" && root.network.mtu <= 0
                        && !root.isWiredConnection)
                }

                Form.SettingRow {
                    objectName: "connectionIpv6Addresses"
                    visible: root.network && root.network.ipv6Addresses.length > 0
                    label: I18n.tr("settings.internet.details.ipv6_address", "IPv6 address")
                    sublabel: visible ? root.network.ipv6Addresses.join("\n") : ""
                    sublabelTextFormat: Text.PlainText
                    isLast: !root.network || (root.network.ipv6Gateway === ""
                        && root.network.hardwareAddress === "" && root.network.mtu <= 0
                        && !root.isWiredConnection)
                }

                Form.SettingRow {
                    objectName: "connectionIpv6Gateway"
                    visible: root.network && root.network.ipv6Gateway !== ""
                    label: I18n.tr("settings.internet.details.ipv6_gateway", "IPv6 gateway")
                    sublabel: visible ? root.network.ipv6Gateway : ""
                    sublabelTextFormat: Text.PlainText
                    isLast: !root.network || (root.network.hardwareAddress === ""
                        && root.network.mtu <= 0 && !root.isWiredConnection)
                }

                Form.SettingRow {
                    objectName: "connectionHardwareAddress"
                    visible: root.network && root.network.hardwareAddress !== ""
                    label: I18n.tr("settings.internet.details.hardware_address", "MAC address")
                    sublabel: visible ? root.network.hardwareAddress : ""
                    sublabelTextFormat: Text.PlainText
                    isLast: !root.network || (root.network.mtu <= 0 && !root.isWiredConnection)
                }

                Form.SettingRow {
                    objectName: "connectionMtu"
                    visible: root.network && root.network.mtu > 0
                    label: I18n.tr("settings.internet.details.mtu", "MTU")
                    sublabel: visible ? String(root.network.mtu) : ""
                    sublabelTextFormat: Text.PlainText
                    isLast: !root.isWiredConnection
                }

                Form.SettingRow {
                    objectName: "connectionLink"
                    visible: root.isWiredConnection
                    label: I18n.tr("settings.internet.details.link", "Link")
                    sublabel: visible ? root.linkText() : ""
                    sublabelTextFormat: Text.PlainText
                    isLast: !root.network || root.network.linkSpeedMbps <= 0
                }

                Form.SettingRow {
                    objectName: "connectionLinkSpeed"
                    visible: root.isWiredConnection && root.network.linkSpeedMbps > 0
                    label: I18n.tr("settings.internet.details.link_speed", "Link speed")
                    sublabel: visible ? root.linkSpeedText(root.network.linkSpeedMbps) : ""
                    sublabelTextFormat: Text.PlainText
                    isLast: true
                }

                Text {
                    objectName: "connectionDetailsEmpty"
                    visible: root.showConnectionDetails && !root.hasConnectionDetailValue
                    Layout.fillWidth: true
                    Layout.margins: Components.Theme.spacingLarge
                    text: I18n.tr("settings.internet.details.empty",
                                  "No runtime connection details are available.")
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("settings.internet.section.wifi", "WI-FI")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 24

            Form.SettingRow {
                label: I18n.tr("settings.internet.wifi.label", "Wi-Fi")
                sublabel: root.network && root.network.wifiPending
                    ? I18n.tr("settings.internet.wifi.pending", "Updating Wi-Fi power…")
                    : I18n.tr("settings.internet.wifi.power_description",
                              "Turn wireless networking on or off.")

                Controls.ToggleSwitch {
                    objectName: "wifiPowerToggle"
                    checked: root.network ? root.network.wifiEnabled : false
                    enabled: root.canSetWifi
                    accessibleName: I18n.tr("settings.internet.wifi.label", "Wi-Fi")
                    onToggled: targetChecked => {
                        if (root.canSetWifi)
                            root.network.setWifiEnabled(targetChecked)
                    }
                }
            }

            Text {
                objectName: "wifiUnavailableMessage"
                Layout.fillWidth: true
                Layout.leftMargin: Components.Theme.spacingLarge
                Layout.rightMargin: Components.Theme.spacingLarge
                Layout.bottomMargin: Components.Theme.spacingMedium
                visible: root.serviceReady && root.network && !root.network.wifiAvailable
                text: I18n.tr("settings.internet.wifi.no_adapter", "No Wi-Fi adapter is available.")
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Text {
                objectName: "wifiHardwareUnavailableMessage"
                Layout.fillWidth: true
                Layout.leftMargin: Components.Theme.spacingLarge
                Layout.rightMargin: Components.Theme.spacingLarge
                Layout.bottomMargin: Components.Theme.spacingMedium
                visible: root.serviceReady && root.network && root.network.wifiAvailable
                    && !root.network.wifiHardwareEnabled
                text: I18n.tr("settings.internet.wifi.hardware_unavailable",
                              "Wireless hardware is disabled or unavailable.")
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Text {
                objectName: "wifiSoftwareDisabledMessage"
                Layout.fillWidth: true
                Layout.leftMargin: Components.Theme.spacingLarge
                Layout.rightMargin: Components.Theme.spacingLarge
                Layout.bottomMargin: Components.Theme.spacingMedium
                visible: root.serviceReady && root.network && root.network.wifiAvailable
                    && root.network.wifiHardwareEnabled && !root.network.wifiEnabled
                text: I18n.tr("settings.internet.wifi.software_disabled", "Wi-Fi is turned off.")
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
                wrapMode: Text.WordWrap
            }

            Form.SettingRow {
                label: I18n.tr("settings.internet.scan.label", "Nearby networks")
                sublabel: root.network && root.network.wifiScanning
                    ? I18n.tr("settings.internet.scan.progress", "Scanning for nearby networks…")
                    : I18n.tr("settings.internet.scan.description",
                              "Refresh the list of visible Wi-Fi networks.")
                isLast: true

                Controls.Button {
                    objectName: "wifiScanButton"
                    label: root.network && root.network.wifiScanning
                        ? I18n.tr("settings.internet.scan.progress", "Scanning for nearby networks…")
                        : I18n.tr("settings.internet.scan.action", "Scan")
                    enabled: root.canScanWifi
                    onClicked: {
                        if (root.canScanWifi)
                            root.network.requestWifiScan()
                    }
                }
            }
        }

        Form.SectionHeader {
            text: I18n.tr("settings.internet.section.available_networks", "AVAILABLE NETWORKS")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            Layout.bottomMargin: 24

            ColumnLayout {
                width: parent.width
                spacing: 0

                ListView {
                    id: wifiList
                    objectName: "availableNetworks"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, 360)
                    clip: true
                    model: root.network ? root.network.wifiModel : null
                    spacing: 0

                    delegate: Item {
                        id: networkRow
                        objectName: "availableNetworkRow"
                        required property string ssid
                        required property int strength
                        required property bool active
                        required property bool secured
                        required property int frequencyMHz
                        required property int index

                        width: ListView.view.width
                        visible: ssid.trim() !== ""
                        height: visible ? Math.max(64, networkRowContent.implicitHeight
                                                   + Components.Theme.spacingMedium * 2) : 0

                        RowLayout {
                            id: networkRowContent
                            anchors.fill: parent
                            anchors.leftMargin: Components.Theme.spacingLarge
                            anchors.rightMargin: Components.Theme.spacingLarge
                            spacing: Components.Theme.spacingMedium

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: Components.Theme.spacingMicro

                                Text {
                                    objectName: "availableNetworkSsid"
                                    Layout.fillWidth: true
                                    text: networkRow.ssid
                                    textFormat: Text.PlainText
                                    color: Components.Theme.textPrimary
                                    font.family: Components.Theme.fontFamily
                                    font.pixelSize: Components.Theme.fontSizeTitle
                                    font.weight: Components.Theme.fontWeightMedium
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Components.Theme.spacingSmall

                                    Text {
                                        objectName: "availableNetworkSecurity"
                                        text: networkRow.secured
                                            ? I18n.tr("settings.internet.network.secured", "Secured")
                                            : I18n.tr("settings.internet.network.open", "Open")
                                        color: Components.Theme.textSecondary
                                        font.family: Components.Theme.fontFamily
                                        font.pixelSize: Components.Theme.fontSizeSmall
                                    }

                                    Text {
                                        objectName: "availableNetworkFrequencyBand"
                                        visible: root.frequencyBandText(networkRow.frequencyMHz) !== ""
                                        text: root.frequencyBandText(networkRow.frequencyMHz)
                                        color: Components.Theme.textSecondary
                                        font.family: Components.Theme.fontFamily
                                        font.pixelSize: Components.Theme.fontSizeSmall
                                    }

                                    Text {
                                        objectName: "availableNetworkActiveState"
                                        visible: networkRow.active
                                        text: I18n.tr("settings.internet.network.connected", "Connected")
                                        color: Components.Theme.accent
                                        font.family: Components.Theme.fontFamily
                                        font.pixelSize: Components.Theme.fontSizeSmall
                                        font.weight: Components.Theme.fontWeightMedium
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.alignment: Qt.AlignVCenter
                                spacing: Components.Theme.spacingTiny

                                Text {
                                    objectName: "availableNetworkSignal"
                                    Layout.alignment: Qt.AlignRight
                                    text: I18n.tr("settings.internet.network.signal",
                                                  "Signal {strength}%")
                                        .replace("{strength}", String(Math.max(0, Math.min(100,
                                                                                          networkRow.strength))))
                                    color: Components.Theme.textSecondary
                                    font.family: Components.Theme.fontFamily
                                    font.pixelSize: Components.Theme.fontSizeSmall
                                }

                                Rectangle {
                                    id: signalTrack
                                    Layout.alignment: Qt.AlignRight
                                    width: 64
                                    height: 4
                                    radius: height / 2
                                    color: Components.Theme.cardBorder

                                    Rectangle {
                                        width: signalTrack.width
                                            * Math.max(0, Math.min(100, networkRow.strength)) / 100
                                        height: parent.height
                                        radius: height / 2
                                        color: Components.Theme.accent
                                    }
                                }
                            }
                        }

                        Rectangle {
                            visible: networkRow.index < wifiList.count - 1
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: Components.Theme.spacingLarge
                            height: 1
                            color: Components.Theme.cardBorder
                        }
                    }
                }

                Text {
                    objectName: "availableNetworksEmpty"
                    Layout.fillWidth: true
                    Layout.preferredHeight: wifiList.contentHeight <= 0 ? 64 : 0
                    visible: wifiList.contentHeight <= 0
                    text: I18n.tr("settings.internet.networks.empty",
                                  "No named Wi-Fi networks are available.")
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSubtitle
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
