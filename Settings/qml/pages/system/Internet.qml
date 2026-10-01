import QtQuick
import QtQuick.Controls as QQC2
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

    readonly property var dnsPresets: [
        { name: "Cloudflare", servers: ["1.1.1.1", "1.0.0.1",
                                         "2606:4700:4700::1111", "2606:4700:4700::1001"] },
        { name: "Quad9", servers: ["9.9.9.9", "149.112.112.112",
                                    "2620:fe::fe", "2620:fe::9"] },
        { name: "Google", servers: ["8.8.8.8", "8.8.4.4",
                                     "2001:4860:4860::8888", "2001:4860:4860::8844"] },
        { name: "Custom", servers: [] }
    ]
    readonly property var dnsPresetNames: root.dnsPresets.map(entry => entry.name)
    property int dnsDraftMode: System.NetworkDnsMode.Advanced
    property string dnsDraftText: ""
    property string dnsSelectedPreset: "Custom"
    property bool dnsDraftDirty: false
    property bool dnsExternalChanged: false
    property bool dnsAwaitingAuthoritative: false
    property bool dnsLoadingDraft: false
    property bool dnsEditorInitialized: false
    property string dnsBaseFingerprint: ""
    property int dnsExpectedMode: System.NetworkDnsMode.Advanced
    property var dnsExpectedValidation: ({ valid: false, ipv4Servers: [], ipv6Servers: [] })
    property string dnsOperationStatus: ""

    readonly property var dnsDraftServers: root.dnsDraftText.split(/[\s,]+/).filter(value => value.length > 0)
    readonly property var dnsDraftValidation: {
        if (root.dnsDraftMode === System.NetworkDnsMode.Automatic)
            return { valid: true, error: "", ipv4Servers: [], ipv6Servers: [] }
        if (root.dnsDraftMode !== System.NetworkDnsMode.Custom || !root.network)
            return { valid: false, error: "Choose Automatic or Custom DNS to apply settings",
                     ipv4Servers: [], ipv6Servers: [] }
        return root.network.validateDnsServers(root.dnsDraftServers)
    }
    readonly property bool dnsDraftMatchesAuthoritative: {
        if (!root.network || root.dnsDraftMode !== root.network.dnsMode)
            return false
        if (root.dnsDraftMode === System.NetworkDnsMode.Automatic)
            return root.network.configuredIpv4DnsServers.length === 0
                && root.network.configuredIpv6DnsServers.length === 0
        if (root.dnsDraftMode === System.NetworkDnsMode.Custom) {
            return root.dnsDraftValidation.valid
                && root.sameDnsServers(root.dnsDraftValidation.ipv4Servers,
                                       root.network.configuredIpv4DnsServers)
                && root.sameDnsServers(root.dnsDraftValidation.ipv6Servers,
                                       root.network.configuredIpv6DnsServers)
        }
        return root.dnsDraftMode === System.NetworkDnsMode.Advanced
    }
    readonly property bool canApplyDns:
        root.serviceReady && root.isConnected && root.network && root.network.dnsEditable
        && !root.network.dnsPending && !root.dnsExternalChanged
        && root.dnsDraftMode !== System.NetworkDnsMode.Advanced
        && root.dnsDraftValidation.valid && !root.dnsDraftMatchesAuthoritative

    function dnsModeText(mode) {
        if (mode === System.NetworkDnsMode.Automatic)
            return I18n.tr("settings.internet.dns.mode.automatic", "Automatic")
        if (mode === System.NetworkDnsMode.Custom)
            return I18n.tr("settings.internet.dns.mode.custom", "Custom")
        return I18n.tr("settings.internet.dns.mode.advanced", "Advanced")
    }

    function dnsAuthorityFingerprint() {
        if (!root.network)
            return ""
        return [root.network.activeConnectionPath, root.network.profilePath,
                root.network.dnsMode, root.network.configuredIpv4DnsServers.join("\n"),
                root.network.configuredIpv6DnsServers.join("\n")].join("|")
    }

    function sameDnsServers(left, right) {
        if (!left || !right || left.length !== right.length)
            return false
        for (let index = 0; index < left.length; ++index) {
            if (left[index] !== right[index])
                return false
        }
        return true
    }

    function sameDnsSet(left, right) {
        if (!left || !right || left.length !== right.length)
            return false
        const first = left.slice().sort()
        const second = right.slice().sort()
        return root.sameDnsServers(first, second)
    }

    function configuredDnsProvider() {
        if (!root.network)
            return "Advanced"
        if (root.network.dnsMode === System.NetworkDnsMode.Automatic)
            return I18n.tr("settings.internet.dns.provider.automatic", "Automatic")
        if (root.network.dnsMode === System.NetworkDnsMode.Advanced)
            return I18n.tr("settings.internet.dns.provider.advanced", "Advanced")
        for (let index = 0; index < 3; ++index) {
            const preset = root.dnsPresets[index]
            const ipv4 = preset.servers.filter(address => address.indexOf(":") < 0)
            const ipv6 = preset.servers.filter(address => address.indexOf(":") >= 0)
            if (root.sameDnsSet(ipv4, root.network.configuredIpv4DnsServers)
                && root.sameDnsSet(ipv6, root.network.configuredIpv6DnsServers))
                return preset.name
        }
        return I18n.tr("settings.internet.dns.provider.custom", "Custom")
    }

    function loadDnsDraft() {
        if (!root.network)
            return
        root.dnsLoadingDraft = true
        root.dnsDraftMode = root.network.dnsMode
        root.dnsDraftText = root.network.configuredIpv4DnsServers.concat(
            root.network.configuredIpv6DnsServers).join("\n")
        root.dnsSelectedPreset = root.configuredDnsProvider()
        root.dnsBaseFingerprint = root.dnsAuthorityFingerprint()
        root.dnsDraftDirty = false
        root.dnsExternalChanged = false
        root.dnsAwaitingAuthoritative = false
        root.dnsEditorInitialized = true
        root.dnsLoadingDraft = false
    }

    function updateDnsDraft() {
        if (root.dnsLoadingDraft)
            return
        root.dnsOperationStatus = ""
        root.dnsDraftDirty = !root.dnsDraftMatchesAuthoritative
    }

    function synchronizeDnsEditor() {
        if (!root.dnsEditorInitialized || (!root.dnsDraftDirty && !root.dnsAwaitingAuthoritative)) {
            root.loadDnsDraft()
            return
        }
        if (root.dnsAwaitingAuthoritative) {
            if (root.network && root.dnsExpectedMode === root.network.dnsMode) {
                const expectedMatches = root.dnsExpectedMode === System.NetworkDnsMode.Automatic
                    ? root.network.configuredIpv4DnsServers.length === 0
                        && root.network.configuredIpv6DnsServers.length === 0
                    : root.dnsExpectedValidation.valid
                        && root.sameDnsServers(root.dnsExpectedValidation.ipv4Servers,
                                               root.network.configuredIpv4DnsServers)
                        && root.sameDnsServers(root.dnsExpectedValidation.ipv6Servers,
                                               root.network.configuredIpv6DnsServers)
                if (expectedMatches)
                    root.loadDnsDraft()
            }
            return
        }
        if (root.dnsAuthorityFingerprint() !== root.dnsBaseFingerprint)
            root.dnsExternalChanged = true
    }

    function selectDnsMode(index) {
        if (index < 0)
            return
        root.dnsDraftMode = index === 0 ? System.NetworkDnsMode.Automatic
                                        : System.NetworkDnsMode.Custom
        if (root.dnsDraftMode === System.NetworkDnsMode.Custom
            && root.dnsDraftText.length === 0 && root.network) {
            root.dnsDraftText = root.network.configuredIpv4DnsServers.concat(
                root.network.configuredIpv6DnsServers).join("\n")
        }
        root.updateDnsDraft()
    }

    function selectDnsPreset(index) {
        if (index < 0 || index >= root.dnsPresets.length)
            return
        root.dnsDraftMode = System.NetworkDnsMode.Custom
        root.dnsSelectedPreset = root.dnsPresets[index].name
        if (index < 3)
            root.dnsDraftText = root.dnsPresets[index].servers.join("\n")
        root.updateDnsDraft()
    }

    function reloadDnsDraft() {
        root.loadDnsDraft()
        root.dnsOperationStatus = ""
    }

    function keepDnsDraft() {
        root.dnsBaseFingerprint = root.dnsAuthorityFingerprint()
        root.dnsExternalChanged = false
        root.dnsDraftDirty = !root.dnsDraftMatchesAuthoritative
    }

    function applyDnsDraft() {
        if (!root.canApplyDns || !root.network)
            return
        const servers = root.dnsDraftMode === System.NetworkDnsMode.Custom
            ? root.dnsDraftValidation.servers : []
        root.dnsExpectedMode = root.dnsDraftMode
        root.dnsExpectedValidation = root.dnsDraftValidation
        if (!root.network.applyDns(root.dnsDraftMode, servers))
            root.dnsOperationStatus = root.network.dnsErrorString
    }

    Component.onCompleted: root.loadDnsDraft()

    Connections {
        target: root.network
        ignoreUnknownSignals: true
        function onDnsChanged() { root.synchronizeDnsEditor() }
        function onDnsOperationFinished(outcome, error) {
            if (outcome === System.NetworkDnsOutcome.Applied) {
                root.dnsOperationStatus = I18n.tr(
                    "settings.internet.dns.result.applied", "DNS settings updated for this connection.")
                root.dnsDraftDirty = false
                root.dnsAwaitingAuthoritative = true
                dnsSuccessTimer.restart()
            } else if (outcome === System.NetworkDnsOutcome.SavedNeedsReconnect) {
                root.dnsOperationStatus = I18n.tr(
                    "settings.internet.dns.result.saved_reconnect",
                    "DNS settings were saved. Reconnect this network to apply them.")
                root.dnsDraftDirty = false
                root.dnsAwaitingAuthoritative = true
            } else if (outcome === System.NetworkDnsOutcome.Conflict) {
                root.dnsOperationStatus = I18n.tr(
                    "settings.internet.dns.result.conflict",
                    "The connection changed while editing. Reload the current settings and try again.")
                root.dnsExternalChanged = true
            } else {
                root.dnsOperationStatus = error
            }
        }
    }

    Timer {
        id: dnsSuccessTimer
        interval: 4000
        repeat: false
        onTriggered: {
            if (root.network && !root.network.dnsNeedsReconnect)
                root.dnsOperationStatus = ""
        }
    }

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
            objectName: "dnsSection"
            visible: root.showConnectionDetails
            text: I18n.tr("settings.internet.section.dns", "DNS")
            Layout.bottomMargin: 12
        }

        Form.FormCard {
            objectName: "dnsCard"
            visible: root.showConnectionDetails
            Layout.bottomMargin: 24

            ColumnLayout {
                width: parent.width
                spacing: 0

                Form.SettingRow {
                    objectName: "dnsConfiguredMode"
                    label: I18n.tr("settings.internet.dns.label.mode", "Mode")
                    sublabel: root.network ? root.dnsModeText(root.network.dnsMode) : "Advanced"
                    sublabelTextFormat: Text.PlainText
                }

                Form.SettingRow {
                    objectName: "dnsProviderValue"
                    label: I18n.tr("settings.internet.dns.label.provider", "Provider")
                    sublabel: root.configuredDnsProvider()
                    sublabelTextFormat: Text.PlainText
                }

                Form.SettingRow {
                    objectName: "dnsActiveServers"
                    label: I18n.tr("settings.internet.dns.label.active_servers",
                                   "DNS servers for this connection")
                    sublabel: root.network
                        ? root.network.activeIpv4DnsServers.concat(
                            root.network.activeIpv6DnsServers).join("\n")
                        : ""
                    sublabelTextFormat: Text.PlainText
                }

                Form.SettingRow {
                    objectName: "dnsModeDraftRow"
                    label: I18n.tr("settings.internet.dns.label.choose_mode", "Choose DNS mode")
                    sublabel: I18n.tr("settings.internet.dns.description.mode",
                                      "Choose how this connection receives DNS servers.")
                    sublabelTextFormat: Text.PlainText
                    isLast: root.dnsDraftMode !== System.NetworkDnsMode.Custom
                    Controls.SelectButton {
                        objectName: "dnsModeSelector"
                        Layout.preferredWidth: 190
                        label: root.dnsModeText(root.dnsDraftMode)
                        options: [root.dnsModeText(System.NetworkDnsMode.Automatic),
                                  root.dnsModeText(System.NetworkDnsMode.Custom)]
                        selectedIndex: root.dnsDraftMode === System.NetworkDnsMode.Automatic ? 0
                                     : root.dnsDraftMode === System.NetworkDnsMode.Custom ? 1 : -1
                        enabled: root.network && root.network.dnsEditable
                            && !root.network.dnsPending && !root.dnsExternalChanged
                        Accessible.name: I18n.tr("settings.internet.dns.label.choose_mode",
                                                 "Choose DNS mode")
                        onSelected: index => root.selectDnsMode(index)
                    }
                }

                Form.SettingRow {
                    objectName: "dnsPresetDraftRow"
                    visible: root.dnsDraftMode === System.NetworkDnsMode.Custom
                    label: I18n.tr("settings.internet.dns.label.preset", "DNS preset")
                    sublabel: I18n.tr("settings.internet.dns.description.preset",
                                      "Select a preset or enter server addresses below.")
                    sublabelTextFormat: Text.PlainText
                    Controls.SelectButton {
                        objectName: "dnsPresetSelector"
                        Layout.preferredWidth: 190
                        label: root.dnsSelectedPreset
                        options: root.dnsPresetNames
                        selectedIndex: Math.max(0, root.dnsPresetNames.indexOf(root.dnsSelectedPreset))
                        enabled: root.network && root.network.dnsEditable
                            && !root.network.dnsPending && !root.dnsExternalChanged
                        Accessible.name: I18n.tr("settings.internet.dns.label.preset", "DNS preset")
                        onSelected: index => root.selectDnsPreset(index)
                    }
                }

                ColumnLayout {
                    visible: root.dnsDraftMode === System.NetworkDnsMode.Custom
                    Layout.fillWidth: true
                    Layout.leftMargin: Components.Theme.spacingLarge
                    Layout.rightMargin: Components.Theme.spacingLarge
                    Layout.topMargin: Components.Theme.spacingMedium
                    Layout.bottomMargin: Components.Theme.spacingMedium
                    spacing: Components.Theme.spacingSmall

                    Text {
                        Layout.fillWidth: true
                        text: I18n.tr("settings.internet.dns.label.custom_servers", "Custom DNS servers")
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeNormal
                        font.weight: Components.Theme.fontWeightMedium
                    }

                    QQC2.TextArea {
                        id: dnsCustomInput
                        objectName: "dnsCustomInput"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 86
                        text: root.dnsDraftText
                        placeholderText: I18n.tr("settings.internet.dns.placeholder.servers",
                            "Enter IP addresses separated by commas, spaces, or new lines")
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        enabled: root.network && root.network.dnsEditable
                            && !root.network.dnsPending && !root.dnsExternalChanged
                        onTextChanged: {
                            if (root.dnsLoadingDraft)
                                return
                            root.dnsDraftText = text
                            root.dnsSelectedPreset = "Custom"
                            root.updateDnsDraft()
                        }
                    }

                    Text {
                        objectName: "dnsValidationError"
                        Layout.fillWidth: true
                        visible: root.dnsDraftMode === System.NetworkDnsMode.Custom
                            && !root.dnsDraftValidation.valid
                        text: visible ? root.dnsDraftValidation.error : ""
                        textFormat: Text.PlainText
                        color: Components.Theme.errorColor
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                    }
                }

                ColumnLayout {
                    visible: root.dnsExternalChanged
                    Layout.fillWidth: true
                    Layout.margins: Components.Theme.spacingLarge
                    spacing: Components.Theme.spacingSmall

                    Text {
                        objectName: "dnsExternalChangeWarning"
                        Layout.fillWidth: true
                        text: I18n.tr("settings.internet.dns.external_changed",
                            "This connection changed while you were editing. Reload its settings or explicitly apply your draft to the current connection.")
                        textFormat: Text.PlainText
                        color: Components.Theme.warningColor
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignRight
                        Controls.Button {
                            objectName: "dnsReloadButton"
                            label: I18n.tr("settings.internet.dns.action.reload", "Reload settings")
                            enabled: root.network && !root.network.dnsPending
                            onClicked: root.reloadDnsDraft()
                        }
                        Controls.Button {
                            objectName: "dnsKeepDraftButton"
                            label: I18n.tr("settings.internet.dns.action.keep_draft",
                                           "Apply my draft to this connection")
                            enabled: root.network && root.network.dnsEditable
                                && !root.network.dnsPending
                            onClicked: root.keepDnsDraft()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Components.Theme.spacingLarge
                    Layout.rightMargin: Components.Theme.spacingLarge
                    Layout.topMargin: Components.Theme.spacingMedium
                    Layout.bottomMargin: Components.Theme.spacingLarge

                    Text {
                        objectName: "dnsOperationStatus"
                        Layout.fillWidth: true
                        visible: root.network && (root.network.dnsPending
                            || root.network.dnsNeedsReconnect || root.dnsOperationStatus !== "")
                        text: root.network && root.network.dnsPending
                            ? I18n.tr("settings.internet.dns.pending", "Updating DNS settings…")
                            : (root.dnsOperationStatus !== "" ? root.dnsOperationStatus
                                                               : root.network.dnsErrorString)
                        textFormat: Text.PlainText
                        color: root.network && root.network.dnsNeedsReconnect
                            ? Components.Theme.warningColor : Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                        wrapMode: Text.WordWrap
                    }

                    Controls.Button {
                        objectName: "dnsApplyButton"
                        label: I18n.tr("settings.internet.dns.action.apply", "Apply")
                        primary: true
                        enabled: root.canApplyDns
                        onClicked: root.applyDnsDraft()
                    }
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
