import QtQuick
import QtQuick.Layouts
import "." as Components
import "controls" as Controls

// Inline status strip for backend-driven pages: shows a busy hint, an error
// with a retry action, or an informational message, each with a leading icon.
Rectangle {
    id: root
    property bool busy: false
    property string error: ""
    property string message: ""
    property string busyText: I18n.tr("settings.notice.busy", "Reading or applying settings…")
    property string actionLabel: I18n.tr("settings.notice.retry", "Retry")
    property string retryObjectName: ""
    property bool showAction: !busy && error.length > 0
    property bool actionPrimary: false
    property string secondaryActionLabel: ""
    signal retry()
    signal secondaryAction()

    readonly property bool isError: !busy && error.length > 0
    readonly property string iconGlyph: busy ? "" : (isError ? "" : "")
    readonly property color iconColor: isError ? Components.Theme.errorColor : Components.Theme.textSecondary

    visible: busy || error.length > 0 || message.length > 0
    Layout.fillWidth: true
    Layout.bottomMargin: visible ? 24 : 0
    implicitHeight: visible ? Math.max(44, noticeRow.implicitHeight + Components.Theme.spacingMedium * 2) : 0
    radius: Components.Theme.controlRadius
    color: isError
        ? Qt.rgba(Components.Theme.errorColor.r, Components.Theme.errorColor.g, Components.Theme.errorColor.b,
                  Components.Theme.isLight ? 0.07 : 0.10)
        : (Components.Theme.isLight ? Qt.rgba(0, 0, 0, 0.025) : Qt.rgba(1, 1, 1, 0.035))
    border.width: 1
    border.color: isError
        ? Qt.rgba(Components.Theme.errorColor.r, Components.Theme.errorColor.g, Components.Theme.errorColor.b, 0.35)
        : Components.Theme.cardBorder

    RowLayout {
        id: noticeRow
        anchors {
            left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter
            leftMargin: Components.Theme.spacingLarge; rightMargin: Components.Theme.spacingMedium
        }
        spacing: Components.Theme.spacingMedium

        Text {
            Layout.alignment: Qt.AlignVCenter
            text: root.iconGlyph
            color: root.iconColor
            font.family: Components.Theme.iconFontFamily
            font.pixelSize: 16
        }

        Text {
            id: noticeText
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: root.busy ? root.busyText : root.error || root.message
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: root.isError ? Components.Theme.textPrimary : Components.Theme.textSecondary
            font.family: Components.Theme.typography.role("caption").family
            font.pixelSize: Components.Theme.typography.role("caption").pixelSize
            lineHeight: Components.Theme.typography.role("caption").lineHeight
            lineHeightMode: Text.ProportionalHeight
            font.letterSpacing: Components.Theme.typography.role("caption").letterSpacing
        }

        Controls.ButtonCapsule {
            objectName: root.retryObjectName.length > 0 ? root.retryObjectName + "Secondary" : ""
            Layout.alignment: Qt.AlignVCenter
            visible: root.secondaryActionLabel.length > 0
            label: root.secondaryActionLabel
            onClicked: root.secondaryAction()
        }

        Controls.ButtonCapsule {
            id: retryButton
            objectName: root.retryObjectName
            Layout.alignment: Qt.AlignVCenter
            visible: root.showAction
            label: root.actionLabel
            primary: root.actionPrimary
            onClicked: root.retry()
        }
    }
}
