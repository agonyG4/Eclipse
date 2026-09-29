import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import "../../components" as Components
import "../../components/controls" as Controls

Item {
    id: root

    property var controller
    property var parameters: []
    property bool enabled: false

    implicitHeight: content.implicitHeight

    function humanize(name) {
        return String(name).replace(/[._-]+/g, " ").replace(/\b\w/g, letter => letter.toUpperCase())
    }

    function componentCount(parameterType) {
        if (parameterType === "vec2") return 2
        if (parameterType === "vec3") return 3
        if (parameterType === "vec4") return 4
        return 0
    }

    function componentName(index) {
        return ["X", "Y", "Z", "W"][index]
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 12

        Repeater {
            model: root.parameters

            delegate: ColumnLayout {
                id: parameterRow
                required property var modelData
                Layout.fillWidth: true
                spacing: 6

                readonly property var descriptor: modelData
                readonly property bool rangedFloat: descriptor.parameterType === "float"
                    && descriptor.range && descriptor.range.type === "float"
                readonly property bool rangedVector: descriptor.range
                    && descriptor.range.type === "float_components"
                readonly property int vectorSize: root.componentCount(descriptor.parameterType)

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: root.humanize(parameterRow.descriptor.name)
                        color: Components.Theme.textPrimary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeNormal
                        font.weight: Components.Theme.fontWeightMedium
                        elide: Text.ElideRight
                    }
                    Text {
                        text: parameterRow.descriptor.overridden
                            ? I18n.tr("apps.settings.pages.visual_effects.overridden", "Override")
                            : I18n.tr("apps.settings.pages.visual_effects.automatic", "Automatic")
                        color: Components.Theme.textSecondary
                        font.family: Components.Theme.fontFamily
                        font.pixelSize: Components.Theme.fontSizeSmall
                    }
                    QQC2.Button {
                        visible: parameterRow.descriptor.overridden
                        enabled: root.enabled
                        text: I18n.tr("apps.settings.pages.visual_effects.reset", "Reset")
                        Accessible.name: I18n.tr("apps.settings.pages.visual_effects.reset_parameter", "Reset parameter")
                        onClicked: root.controller.clearMaterialProgramParameter(parameterRow.descriptor.name)
                    }
                }

                Controls.Slider {
                    visible: parameterRow.rangedFloat
                    Layout.fillWidth: true
                    from: parameterRow.rangedFloat ? parameterRow.descriptor.range.min : 0
                    to: parameterRow.rangedFloat ? parameterRow.descriptor.range.max : 1
                    stepSize: Math.max(0.001, (to - from) / 100)
                    showEndpointGlyphs: false
                    enabled: root.enabled
                    modelValueEnabled: true
                    modelValue: Number(parameterRow.descriptor.value)
                    value: modelValue
                    detentEnabled: true
                    detentValue: Number(parameterRow.descriptor.default)
                    valueText: Number(displayedValue).toFixed(3)
                    Accessible.name: root.humanize(parameterRow.descriptor.name)
                    onValueEdited: value => root.controller.setMaterialProgramFloatParameter(parameterRow.descriptor.name, value)
                    onPressedChanged: if (!pressed && root.controller) root.controller.flushMaterialProgramParameters()
                    Keys.onReleased: event => {
                        if (event.key === Qt.Key_Left || event.key === Qt.Key_Right
                                || event.key === Qt.Key_Up || event.key === Qt.Key_Down
                                || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown
                                || event.key === Qt.Key_Home || event.key === Qt.Key_End)
                            root.controller.flushMaterialProgramParameters()
                    }
                }

                QQC2.TextField {
                    id: floatInput
                    visible: parameterRow.descriptor.parameterType === "float" && !parameterRow.rangedFloat
                    Layout.fillWidth: true
                    enabled: root.enabled
                    selectByMouse: true
                    validator: DoubleValidator { decimals: 8; notation: DoubleValidator.StandardNotation }
                    Accessible.name: root.humanize(parameterRow.descriptor.name)
                    onEditingFinished: {
                        const value = Number(text)
                        if (acceptableInput && Number.isFinite(value)) {
                            root.controller.setMaterialProgramFloatParameter(parameterRow.descriptor.name, value)
                            root.controller.flushMaterialProgramParameters()
                        }
                    }
                    Binding {
                        target: floatInput
                        property: "text"
                        value: String(parameterRow.descriptor.value)
                        when: !floatInput.activeFocus
                    }
                }

                QQC2.SpinBox {
                    id: integerInput
                    visible: parameterRow.descriptor.parameterType === "int"
                    Layout.fillWidth: true
                    enabled: root.enabled
                    from: parameterRow.descriptor.range && parameterRow.descriptor.range.type === "int"
                        ? parameterRow.descriptor.range.min : -2147483648
                    to: parameterRow.descriptor.range && parameterRow.descriptor.range.type === "int"
                        ? parameterRow.descriptor.range.max : 2147483647
                    value: Number(parameterRow.descriptor.value)
                    editable: true
                    Accessible.name: root.humanize(parameterRow.descriptor.name)
                    onValueModified: {
                        root.controller.setMaterialProgramIntParameter(parameterRow.descriptor.name, value)
                        root.controller.flushMaterialProgramParameters()
                    }
                    Binding {
                        target: integerInput
                        property: "value"
                        value: Number(parameterRow.descriptor.value)
                        when: !integerInput.activeFocus
                    }
                }

                Repeater {
                    model: parameterRow.vectorSize

                    delegate: RowLayout {
                        required property int index
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.preferredWidth: 18
                            text: root.componentName(index)
                            color: Components.Theme.textSecondary
                            font.family: Components.Theme.fontFamily
                            font.pixelSize: Components.Theme.fontSizeSmall
                        }

                        Controls.Slider {
                            readonly property bool componentIsRanged: parameterRow.rangedVector
                                && parameterRow.descriptor.range.components === parameterRow.vectorSize
                            visible: componentIsRanged
                            Layout.fillWidth: true
                            from: componentIsRanged ? parameterRow.descriptor.range.min[index] : 0
                            to: componentIsRanged ? parameterRow.descriptor.range.max[index] : 1
                            stepSize: Math.max(0.001, (to - from) / 100)
                            showEndpointGlyphs: false
                            enabled: root.enabled
                            modelValueEnabled: true
                            modelValue: Number(parameterRow.descriptor.value[index])
                            value: modelValue
                            detentEnabled: true
                            detentValue: Number(parameterRow.descriptor.default[index])
                            valueText: Number(displayedValue).toFixed(3)
                            Accessible.name: root.humanize(parameterRow.descriptor.name) + " " + root.componentName(index)
                            onValueEdited: value => root.controller.setMaterialProgramVectorComponent(parameterRow.descriptor.name, index, value)
                            onPressedChanged: if (!pressed && root.controller) root.controller.flushMaterialProgramParameters()
                        }

                        QQC2.TextField {
                            id: vectorInput
                            visible: !(parameterRow.rangedVector
                                && parameterRow.descriptor.range.components === parameterRow.vectorSize)
                            Layout.fillWidth: true
                            enabled: root.enabled
                            selectByMouse: true
                            validator: DoubleValidator { decimals: 8; notation: DoubleValidator.StandardNotation }
                            Accessible.name: root.humanize(parameterRow.descriptor.name) + " " + root.componentName(index)
                            onEditingFinished: {
                                const value = Number(text)
                                if (acceptableInput && Number.isFinite(value)) {
                                    root.controller.setMaterialProgramVectorComponent(parameterRow.descriptor.name, index, value)
                                    root.controller.flushMaterialProgramParameters()
                                }
                            }
                            Binding {
                                target: vectorInput
                                property: "text"
                                value: String(parameterRow.descriptor.value[index])
                                when: !vectorInput.activeFocus
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 3
                    height: 1
                    color: Components.Theme.cardBorder
                    opacity: 0.55
                }
            }
        }

        Text {
            visible: root.parameters.length === 0
            Layout.fillWidth: true
            text: I18n.tr("apps.settings.pages.visual_effects.no_parameters", "This program has no editable parameters.")
            color: Components.Theme.textSecondary
            font.family: Components.Theme.fontFamily
            font.pixelSize: Components.Theme.fontSizeSmall
            wrapMode: Text.WordWrap
        }
    }
}
