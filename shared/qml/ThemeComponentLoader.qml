import QtQuick

Item {
    id: root

    property url source
    property Component fallbackComponent
    property var initialProperties: ({})
    property var validateItem: null
    readonly property Item item: externalAccepted ? external.item : fallback.item
    implicitWidth: item ? item.width : 0
    implicitHeight: item ? item.height : 0
    readonly property bool usingFallback: !externalAccepted
    readonly property string error: loadError

    property bool externalAccepted: false
    property string loadError: ""

    function reload() {
        externalAccepted = false
        loadError = ""
        external.active = false
        if (source && String(source).length > 0)
            external.setSource(source, initialProperties || ({}))
        else
            external.source = ""
        external.active = true
    }

    onSourceChanged: reload()
    onInitialPropertiesChanged: if (source) reload()
    onValidateItemChanged: if (external.item) inspectExternal()
    function inspectExternal() {
        var candidate = external.item
        var valid = candidate && candidate instanceof Item
        if (valid && validateItem) {
            try { valid = !!validateItem(candidate) }
            catch (exception) { valid = false }
        }
        if (valid) {
            candidate.parent = root
            candidate.x = 0
            candidate.y = 0
            externalAccepted = true
            loadError = ""
        } else {
            loadError = "Theme component root or API contract is invalid"
            externalAccepted = false
            external.active = false
            external.source = ""
            external.active = true
        }
    }

    Loader {
        id: fallback
        sourceComponent: root.fallbackComponent
        active: !root.externalAccepted
    }

    Loader {
        id: external
        active: false
        onLoaded: root.inspectExternal()
        onStatusChanged: {
            if (status === Loader.Error) {
                root.loadError = "Theme component failed to load"
                root.externalAccepted = false
            }
        }
    }
}
