import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import ".." as Components
import "../controls" as Controls
import "../form" as Form

ColumnLayout {
    id: root
    property var manager: null
    property string ruleAction: "allow"
    property string ruleProtocol: "tcp"
    property string pendingAction: ""
    property var pendingData: ({})
    property string pendingDescription: ""
    property alias confirmationDialog: confirmation

    Layout.fillWidth: true
    spacing: Components.Theme.spacingMedium

    function confirm(action, data, description) {
        if (!manager || !manager.loaded || manager.busy)
            return
        pendingAction = action
        pendingData = data
        pendingDescription = description
        confirmation.open()
    }

    Form.SectionHeader { text: I18n.tr("settings.firewall.firewall", "FIREWALL") }

    Text {
        objectName: "firewallAgentHint"
        visible: root.manager && root.manager.knownGraphicalAgentInstalled === false
        Layout.fillWidth: true
        Layout.bottomMargin: Components.Theme.spacingSmall
        text: I18n.tr("settings.firewall.no_graphical_polkit_authentication_agent_det", "No graphical Polkit authentication agent detected. Install and start an agent such as lxqt-policykit to display the password prompt.")
        textFormat: Text.PlainText
        color: Components.Theme.textSecondary
        font.family: Components.Theme.typography.role("caption").family
        font.pixelSize: Components.Theme.typography.role("caption").pixelSize
        lineHeight: Components.Theme.typography.role("caption").lineHeight
        lineHeightMode: Text.ProportionalHeight
        font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
        wrapMode: Text.WordWrap
    }

    Form.FormCard {
        Layout.bottomMargin: Components.Theme.spacingLarge

        Form.SettingRow {
            label: "UFW"
            sublabel: root.manager && root.manager.loaded
                ? I18n.tr("settings.firewall.last_verified_configuration", "Last verified configuration") : I18n.tr("settings.firewall.administrative_access_required", "Administrative access required")
            isLast: !root.manager || !root.manager.loaded

            RowLayout {
                spacing: Components.Theme.spacingSmall
                Text {
                    objectName: "firewallVerifiedStatus"
                    text: !root.manager || !root.manager.loaded ? I18n.tr("settings.firewall.not_inspected", "Not inspected")
                        : root.manager.enabled ? I18n.tr("settings.firewall.active", "Active") : I18n.tr("settings.firewall.inactive", "Inactive")
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.typography.role("caption").family
                    font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                    lineHeight: Components.Theme.typography.role("caption").lineHeight
                    lineHeightMode: Text.ProportionalHeight
                    font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                }
                Controls.Button {
                    objectName: "firewallAuthorizeButton"
                    label: root.manager && root.manager.loaded ? I18n.tr("settings.firewall.refresh", "Refresh") : I18n.tr("settings.firewall.authorize", "Authorize")
                    enabled: !!root.manager && !root.manager.busy
                    onClicked: root.manager.refresh()
                }
            }
        }

        Form.SettingRow {
            visible: !!root.manager && root.manager.loaded
            label: I18n.tr("settings.firewall.firewall_2", "Firewall")
            sublabel: I18n.tr("settings.firewall.filter_incoming_and_outgoing_connections", "Filter incoming and outgoing connections")
            Controls.ToggleSwitch {
                objectName: "firewallEnableToggle"
                checked: !!root.manager && root.manager.enabled
                enabled: !!root.manager && root.manager.loaded && !root.manager.busy
                accessibleName: I18n.tr("settings.firewall.enable_firewall", "Enable firewall")
                onToggled: value => {
                    if (value !== root.manager.enabled)
                        root.confirm("enabled", {enabled: value},
                            value ? I18n.tr("settings.firewall.enable_the_firewall", "Enable the firewall?") :
                                I18n.tr("settings.firewall.disable_the_firewall_network_filtering_will", "Disable the firewall? Network filtering will stop."))
                }
            }
        }

        Form.SettingRow {
            visible: !!root.manager && root.manager.loaded
            label: I18n.tr("settings.firewall.incoming", "Incoming")
            sublabel: I18n.tr("settings.firewall.default_rule_for_incoming_connections", "Default rule for incoming connections")
            Controls.SelectButton {
                objectName: "firewallIncomingPolicy"
                width: 150
                label: !root.manager || !root.manager.incomingPolicy ? I18n.tr("settings.firewall.unknown", "Unknown")
                    : root.manager.incomingPolicy === "deny" ? I18n.tr("settings.firewall.block", "Block") : I18n.tr("settings.firewall.allow", "Allow")
                options: [I18n.tr("settings.firewall.block", "Block"), I18n.tr("settings.firewall.allow", "Allow")]
                selectedIndex: root.manager && root.manager.incomingPolicy === "deny" ? 0
                    : root.manager && root.manager.incomingPolicy === "allow" ? 1 : -1
                enabled: !!root.manager && root.manager.loaded && !root.manager.busy
                    && root.manager.incomingPolicy !== ""
                onSelected: index => {
                    const policy = index === 0 ? "deny" : "allow"
                    if (policy !== root.manager.incomingPolicy)
                        root.confirm("policy", {direction: "incoming", policy: policy},
                            policy === "allow"
                                ? I18n.tr("settings.firewall.allow_all_incoming_connections_by_default_th", "Allow all incoming connections by default? This reduces protection.")
                                : I18n.tr("settings.firewall.block_incoming_connections_by_default", "Block incoming connections by default?"))
                }
            }
        }

        Form.SettingRow {
            visible: !!root.manager && root.manager.loaded
            label: I18n.tr("settings.firewall.outgoing", "Outgoing")
            sublabel: I18n.tr("settings.firewall.default_rule_for_outgoing_connections", "Default rule for outgoing connections")
            Controls.SelectButton {
                objectName: "firewallOutgoingPolicy"
                width: 150
                label: !root.manager || !root.manager.outgoingPolicy ? I18n.tr("settings.firewall.unknown", "Unknown")
                    : root.manager.outgoingPolicy === "deny" ? I18n.tr("settings.firewall.block", "Block") : I18n.tr("settings.firewall.allow", "Allow")
                options: [I18n.tr("settings.firewall.block", "Block"), I18n.tr("settings.firewall.allow", "Allow")]
                selectedIndex: root.manager && root.manager.outgoingPolicy === "deny" ? 0
                    : root.manager && root.manager.outgoingPolicy === "allow" ? 1 : -1
                enabled: !!root.manager && root.manager.loaded && !root.manager.busy
                    && root.manager.outgoingPolicy !== ""
                onSelected: index => {
                    const policy = index === 0 ? "deny" : "allow"
                    if (policy !== root.manager.outgoingPolicy)
                        root.confirm("policy", {direction: "outgoing", policy: policy},
                            policy === "deny"
                                ? I18n.tr("settings.firewall.block_outgoing_connections_by_default_apps_m", "Block outgoing connections by default? Apps may lose connectivity.")
                                : I18n.tr("settings.firewall.allow_outgoing_connections_by_default", "Allow outgoing connections by default?"))
                }
            }
        }

        Form.SettingRow {
            visible: !!root.manager && root.manager.loaded
            label: I18n.tr("settings.firewall.logging", "Logging")
            sublabel: I18n.tr("settings.firewall.record_firewall_events", "Record firewall events")
            isLast: true
            Controls.ToggleSwitch {
                objectName: "firewallLoggingToggle"
                checked: !!root.manager && root.manager.loggingEnabled
                enabled: !!root.manager && root.manager.loaded && !root.manager.busy
                accessibleName: I18n.tr("settings.firewall.enable_firewall_logging", "Enable firewall logging")
                onToggled: value => {
                    if (value !== root.manager.loggingEnabled)
                        root.confirm("logging", {enabled: value},
                            value ? I18n.tr("settings.firewall.enable_firewall_logging_2", "Enable firewall logging?") : I18n.tr("settings.firewall.disable_firewall_logging", "Disable firewall logging?"))
                }
            }
        }
    }

    Text {
        objectName: "firewallManagerError"
        visible: !!root.manager && root.manager.errorText !== ""
        Layout.fillWidth: true
        text: root.manager ? root.manager.errorText : ""
        textFormat: Text.PlainText
        color: Components.Theme.errorColor
        font.family: Components.Theme.typography.role("caption").family
        font.pixelSize: Components.Theme.typography.role("caption").pixelSize
        lineHeight: Components.Theme.typography.role("caption").lineHeight
        lineHeightMode: Text.ProportionalHeight
        font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
        wrapMode: Text.WordWrap
    }

    Form.SectionHeader {
        visible: !!root.manager && root.manager.loaded
        text: I18n.tr("settings.firewall.incoming_rules", "INCOMING RULES")
    }

    Form.FormCard {
        visible: !!root.manager && root.manager.loaded
        Layout.bottomMargin: Components.Theme.spacingLarge

        Text {
            visible: !!root.manager && root.manager.rules.length === 0
            Layout.fillWidth: true
            Layout.margins: Components.Theme.spacingLarge
            text: I18n.tr("settings.firewall.no_explicit_incoming_rules", "No explicit incoming rules.")
            color: Components.Theme.textSecondary
            font.family: Components.Theme.typography.role("caption").family
            font.pixelSize: Components.Theme.typography.role("caption").pixelSize
            lineHeight: Components.Theme.typography.role("caption").lineHeight
            lineHeightMode: Text.ProportionalHeight
            font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
        }

        Repeater {
            model: root.manager && root.manager.loaded ? root.manager.rules : []
            Form.SettingRow {
                readonly property var rule: modelData
                label: rule.action.toUpperCase() + "  " + rule.target
                labelTextFormat: Text.PlainText
                sublabel: I18n.tr("settings.firewall.from_source", "From %1").replace("%1", rule.source)
                sublabelTextFormat: Text.PlainText
                isLast: index === root.manager.rules.length - 1

                Controls.Button {
                    objectName: "firewallRemoveRule_" + index
                    visible: rule.removable
                    label: I18n.tr("settings.firewall.remove", "Remove")
                    danger: true
                    flat: true
                    enabled: root.manager && !root.manager.busy
                    onClicked: root.confirm("removeRule",
                        {action: rule.action, port: rule.port,
                         protocol: rule.protocol, source: rule.sourceSpec},
                        I18n.tr("settings.firewall.remove_rule", "Remove the %1 rule for %2?")
                            .replace("%1", rule.action).replace("%2", rule.target))
                }
            }
        }
    }

    Form.SectionHeader {
        visible: !!root.manager && root.manager.loaded
        text: I18n.tr("settings.firewall.add_incoming_rule", "ADD INCOMING RULE")
    }

    Form.FormCard {
        visible: !!root.manager && root.manager.loaded
        Layout.bottomMargin: Components.Theme.spacingLarge
        margins: Components.Theme.spacingLarge

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Components.Theme.spacingMedium
            RowLayout {
                Layout.fillWidth: true
                spacing: Components.Theme.spacingMedium

                Controls.SelectButton {
                    objectName: "firewallRuleAction"
                    Layout.preferredWidth: 115
                    label: root.ruleAction === "allow" ? I18n.tr("settings.firewall.allow", "Allow") : I18n.tr("settings.firewall.block", "Block")
                    options: [I18n.tr("settings.firewall.allow", "Allow"), I18n.tr("settings.firewall.block", "Block")]
                    selectedIndex: root.ruleAction === "allow" ? 0 : 1
                    onSelected: index => root.ruleAction = index === 0 ? "allow" : "deny"
                }

                QQC2.TextField {
                    id: portField
                    objectName: "firewallRulePort"
                    Layout.fillWidth: true
                    placeholderText: I18n.tr("settings.firewall.port", "Port")
                    placeholderTextColor: Components.Theme.textTertiary
                    Layout.preferredHeight: 36
                    background: Rectangle {
                        radius: Components.Theme.controlRadius
                        color: Components.Theme.cardBg
                        border.width: 1
                        border.color: portField.activeFocus
                            ? Components.Theme.accent : Components.Theme.cardBorder
                    }
                    leftPadding: Components.Theme.spacingMedium
                    validator: IntValidator { bottom: 1; top: 65535 }
                    maximumLength: 5
                    selectByMouse: true
                    color: Components.Theme.textPrimary
                    font.family: Components.Theme.fontFamily
                }

                Controls.SelectButton {
                    objectName: "firewallRuleProtocol"
                    Layout.preferredWidth: 90
                    label: root.ruleProtocol.toUpperCase()
                    options: ["TCP", "UDP"]
                    selectedIndex: root.ruleProtocol === "tcp" ? 0 : 1
                    onSelected: index => root.ruleProtocol = index === 0 ? "tcp" : "udp"
                }
            }

            QQC2.TextField {
                id: sourceField
                objectName: "firewallRuleSource"
                Layout.fillWidth: true
                placeholderText: I18n.tr("settings.firewall.source_ip_cidr_leave_blank_for_any", "Source IP/CIDR (leave blank for any)")
                placeholderTextColor: Components.Theme.textTertiary
                Layout.preferredHeight: 36
                background: Rectangle {
                    radius: Components.Theme.controlRadius
                    color: Components.Theme.cardBg
                    border.width: 1
                    border.color: sourceField.activeFocus
                        ? Components.Theme.accent : Components.Theme.cardBorder
                }
                leftPadding: Components.Theme.spacingMedium
                selectByMouse: true
                color: Components.Theme.textPrimary
                font.family: Components.Theme.fontFamily
            }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: I18n.tr("settings.firewall.administrator_approval_required", "Administrator approval required")
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.typography.role("caption").family
                    font.pixelSize: Components.Theme.typography.role("caption").pixelSize
                    lineHeight: Components.Theme.typography.role("caption").lineHeight
                    lineHeightMode: Text.ProportionalHeight
                    font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
                    wrapMode: Text.WordWrap
                }
                Controls.Button {
                    objectName: "firewallAddRuleButton"
                    label: I18n.tr("settings.firewall.add_rule", "Add rule")
                    primary: true
                    enabled: root.manager && root.manager.loaded && !root.manager.busy
                        && portField.acceptableInput
                    onClicked: root.confirm("addRule",
                        {action: root.ruleAction, port: Number(portField.text),
                         protocol: root.ruleProtocol, source: sourceField.text.trim()},
                        I18n.tr("settings.firewall.add_rule_confirm", "Add %1 for port %2/%3 from %4?")
                            .replace("%1", root.ruleAction).replace("%2", portField.text)
                            .replace("%3", root.ruleProtocol)
                            .replace("%4", sourceField.text.trim()
                                     || I18n.tr("settings.firewall.any_address", "any address")))
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        text: I18n.tr("settings.firewall.changing_policies_may_block_remote_access_re", "Changing policies may block remote access. Review existing rules before applying changes.")
        color: Components.Theme.textSecondary
        font.family: Components.Theme.typography.role("caption").family
        font.pixelSize: Components.Theme.typography.role("caption").pixelSize
        lineHeight: Components.Theme.typography.role("caption").lineHeight
        lineHeightMode: Text.ProportionalHeight
        font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
        wrapMode: Text.WordWrap
    }

    QQC2.Dialog {
        id: confirmation
        objectName: "firewallConfirmationDialog"
        modal: true
        width: Math.max(300, Math.min(430, root.width - 24))
        padding: Components.Theme.spacingLarge
        closePolicy: QQC2.Popup.CloseOnEscape
        anchors.centerIn: QQC2.Overlay.overlay
        background: Rectangle {
            radius: Components.Theme.cardRadius
            color: Components.Theme.cardBg
            border.width: 1
            border.color: Components.Theme.cardBorder
        }
        contentItem: ColumnLayout {
            spacing: Components.Theme.spacingLarge
            Text {
                Layout.fillWidth: true
                text: root.pendingDescription
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                color: Components.Theme.textPrimary
                font.family: Components.Theme.typography.role("label").family
                font.pixelSize: Components.Theme.typography.role("label").pixelSize
                lineHeight: Components.Theme.typography.role("label").lineHeight
                lineHeightMode: Text.ProportionalHeight
                font.letterSpacing: Components.Theme.typography.role("label").letterSpacing
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Components.Theme.spacingMedium
                Controls.Button {
                    label: I18n.tr("settings.firewall.cancel", "Cancel")
                    flat: true
                    onClicked: confirmation.reject()
                }
                Controls.Button {
                    objectName: "firewallConfirmAction"
                    label: I18n.tr("settings.firewall.apply", "Apply")
                    danger: root.pendingAction === "enabled" && !root.pendingData.enabled
                        || root.pendingAction === "policy" && root.pendingData.policy === "allow"
                            && root.pendingData.direction === "incoming"
                    onClicked: confirmation.accept()
                }
            }
        }
        onAccepted: {
            if (!root.manager || !root.manager.loaded || root.manager.busy)
                return
            const data = root.pendingData
            if (root.pendingAction === "enabled")
                root.manager.setEnabled(data.enabled)
            else if (root.pendingAction === "policy")
                root.manager.setDefaultPolicy(data.direction, data.policy)
            else if (root.pendingAction === "logging")
                root.manager.setLogging(data.enabled)
            else if (root.pendingAction === "addRule")
                root.manager.addRule(data.action, data.port, data.protocol, data.source)
            else if (root.pendingAction === "removeRule")
                root.manager.removeRule(data.action, data.port, data.protocol, data.source)
        }
    }
}
