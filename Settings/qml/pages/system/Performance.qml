import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls
import "../../components/form" as Form

Item {
    id: root
    objectName: "performancePage"

    property int selectedProfile: 1
    property int selectedScheduler: 0
    property int selectedMode: 0
    property bool advancedSchedulers: false
    property bool gamemodeEnabled: true

    readonly property var profiles: ["Economy", "Balanced", "Performance"]
    readonly property var schedulers: [
        { name: "Bpfland", kind: "Recommended", detail: "Balanced frame pacing for gaming, desktop and multimedia." },
        { name: "LAVD", kind: "Recommended", detail: "Low-latency scheduling for interactive workloads." },
        { name: "Flash", kind: "Recommended", detail: "Responsive desktop scheduling with quick wakeups." },
        { name: "P2DQ", kind: "Recommended", detail: "Throughput-focused scheduling with selectable modes." }
    ]
    readonly property var advanced: [
        { name: "BORE", detail: "Example advanced scheduler · burst-oriented responsiveness." },
        { name: "Rusty", detail: "Example advanced scheduler · experimental workload testing." }
    ]
    readonly property var modes: [
        { name: "Auto", detail: "Let the scheduler choose its default policy." },
        { name: "Gaming", detail: "Prioritize frame pacing and interactive response." },
        { name: "Low latency", detail: "Favor quick wakeups for latency-sensitive tasks." },
        { name: "Power save", detail: "Reduce power draw and heat where possible." },
        { name: "Server", detail: "Favor steady throughput for sustained work." }
    ]

    Form.ScrollPage {
        anchors.fill: parent
        contentMargins: 24
        maxWidth: 840

        Rectangle {
            objectName: "performancePreviewNotice"
            Layout.fillWidth: true
            Layout.bottomMargin: 22
            implicitHeight: 34
            radius: Components.Theme.controlRadius
            color: Components.Theme.isLight ? Qt.rgba(0, 0, 0, 0.025) : Qt.rgba(1, 1, 1, 0.035)
            border.width: 1
            border.color: Components.Theme.cardBorder
            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 12
                text: "Visual preview · Sample data, local changes"
                color: Components.Theme.textSecondary
                font.family: Components.Theme.fontFamily
                font.pixelSize: Components.Theme.fontSizeSmall
            }
        }

        Form.SectionHeader { text: "PERFORMANCE"; Layout.bottomMargin: 10 }
        Form.SectionHeader { text: "POWER PROFILE"; Layout.bottomMargin: 10 }
        Form.FormCard {
            Layout.bottomMargin: 22
            Form.SettingRow {
                objectName: "preferredPowerProfileRow"
                label: "Preferred profile"
                sublabel: "Affects the balance between responsiveness, battery life and heat."
                isLast: true
                Controls.SelectButton {
                    objectName: "powerProfileSelect"
                    width: 156
                    options: root.profiles
                    selectedIndex: root.selectedProfile
                    label: root.profiles[root.selectedProfile]
                    onSelected: index => root.selectedProfile = index
                }
            }
        }

        Form.SectionHeader { text: "SCHED-EXT"; Layout.bottomMargin: 10 }
        Form.FormCard {
            Layout.bottomMargin: 22
            Form.SettingRow {
                label: "Active scheduler"
                sublabel: "Current CPU scheduling policy."
                Text {
                    text: "Fair scheduler"
                    color: Components.Theme.textSecondary
                    font.family: Components.Theme.fontFamily
                    font.pixelSize: Components.Theme.fontSizeSmall
                }
            }
            Form.SettingRow {
                objectName: "schedulerApplyRow"
                label: "Apply sched-ext profile"
                sublabel: "Selected scheduler: " + root.schedulers[root.selectedScheduler].name
                Controls.ButtonCapsule {
                    objectName: "applySchedulerPreviewButton"
                    label: "Apply"
                    enabled: false
                    Accessible.name: "Apply scheduler profile"
                }
                isLast: true
            }
        }

        Form.SectionHeader { text: "RECOMMENDED SCHEDULERS"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "recommendedSchedulersCard"
            Layout.bottomMargin: 22
            Repeater {
                model: root.schedulers
                delegate: Form.SettingRow {
                    required property var modelData
                    required property int index
                    clickable: true
                    label: modelData.name
                    sublabel: modelData.detail
                    isLast: index === root.schedulers.length - 1
                    Text {
                        text: index === root.selectedScheduler ? "Selected" : "Choose"
                        color: index === root.selectedScheduler ? Components.Theme.accent : Components.Theme.textTertiary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }
                    onClicked: root.selectedScheduler = index
                }
            }
        }

        Form.FormCard {
            Layout.bottomMargin: 22
            Form.SettingRow {
                label: root.advancedSchedulers ? "Hide advanced schedulers" : "Show advanced schedulers"
                sublabel: "Experimental or workload-specific choices for testing."
                clickable: true
                Controls.ButtonCapsule {
                    label: root.advancedSchedulers ? "Hide" : "Show"
                    onClicked: root.advancedSchedulers = !root.advancedSchedulers
                }
                onClicked: root.advancedSchedulers = !root.advancedSchedulers
                isLast: !root.advancedSchedulers
            }
            Repeater {
                model: root.advancedSchedulers ? root.advanced : []
                delegate: Form.SettingRow {
                    required property var modelData
                    label: modelData.name
                    sublabel: modelData.detail
                }
            }
        }

        Form.SectionHeader { text: "SCHEDULER MODE"; Layout.bottomMargin: 10 }
        Form.FormCard {
            objectName: "schedulerModesCard"
            Layout.bottomMargin: 22
            Repeater {
                model: root.modes
                delegate: Form.SettingRow {
                    required property var modelData
                    required property int index
                    clickable: true
                    label: modelData.name
                    sublabel: modelData.detail
                    isLast: index === root.modes.length - 1
                    Text {
                        text: index === root.selectedMode ? "Selected" : ""
                        color: Components.Theme.accent
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }
                    onClicked: root.selectedMode = index
                }
            }
        }

        Form.SectionHeader { text: "SYSTEM BEHAVIOR"; Layout.bottomMargin: 10 }
        Form.FormCard {
            Form.SettingRow {
                objectName: "gamemodeRow"
                label: "Gamemode"
                sublabel: "Request performance hints while a game is active."
                Controls.ToggleSwitch {
                    accessibleName: "Gamemode"
                    checked: root.gamemodeEnabled
                    onToggled: checked => root.gamemodeEnabled = checked
                }
                isLast: true
            }
        }
    }
}
