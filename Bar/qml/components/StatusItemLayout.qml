import QtQuick

Item {
    id: layout
    objectName: "statusItemLayout"

    property var orderStore: null
    property var popupController: null
    property int itemSpacing: 0
    property int holdInterval: 500
    property int dragThreshold: 8
    readonly property bool dragActive: draggedItem !== null
    readonly property string draggedKey: draggedItem ? draggedItem.key : ""
    readonly property alias contentItem: itemHost
    default property alias contentData: itemHost.data

    property var entries: []
    property var localOrder: ["builtin:network", "builtin:bluetooth", "builtin:volume", "builtin:clock"]
    property var orderedEntries: []
    property var previewEntries: []
    property var draggedItem: null
    property real dragPointerX: 0
    property bool dragMoved: false

    implicitWidth: orderedEntries.reduce((sum, entry) => sum + entry.item.width, 0)
        + Math.max(0, orderedEntries.length - 1) * itemSpacing
    implicitHeight: orderedEntries.reduce((height, entry) => Math.max(height, entry.item.height), 0)
    width: implicitWidth
    height: implicitHeight

    function keyIndex(key) {
        const order = orderStore && orderStore.order ? orderStore.order : localOrder
        const index = order.indexOf(key)
        return index < 0 ? Number.MAX_SAFE_INTEGER : index
    }

    function resolveKey(item, key, addressKey) {
        let candidate = key
        const duplicate = entries.find(entry => entry.item !== item && entry.key === candidate)
        if (duplicate && addressKey) {
            const servicePart = addressKey.split("|")[0]
            candidate += ":" + encodeURIComponent(servicePart)
        }
        return candidate
    }

    function registerItem(item, key, addressKey) {
        if (!item || !key)
            return ""
        let entry = entries.find(candidate => candidate.item === item)
        const resolved = resolveKey(item, key, addressKey || "")
        if (entry) {
            if (entry.key !== resolved) {
                entry.key = resolved
                entry.addressKey = addressKey || ""
            }
        } else {
            entry = { item: item, key: resolved, addressKey: addressKey || "" }
            entries = entries.concat([entry])
        }
        if (orderStore && orderStore.registerItem)
            orderStore.registerItem(resolved)
        if (!localOrder.includes(resolved)) {
            if (resolved.startsWith("tray:") || resolved.startsWith("tray-fallback:")) {
                let firstBuiltin = localOrder.length
                for (const builtin of ["builtin:network", "builtin:bluetooth",
                                       "builtin:volume", "builtin:clock"]) {
                    const index = localOrder.indexOf(builtin)
                    if (index >= 0)
                        firstBuiltin = Math.min(firstBuiltin, index)
                }
                localOrder.splice(firstBuiltin, 0, resolved)
            } else {
                localOrder.push(resolved)
            }
        }
        if (item.parent !== itemHost)
            item.parent = itemHost
        rebuild()
        return resolved
    }

    function unregisterItem(item, key) {
        const entry = entries.find(candidate => candidate.item === item
            && (!key || candidate.key === key || candidate.addressKey === key))
        if (!entry)
            return
        if (draggedItem === entry)
            endDrag(item, entry.key, false)
        entries = entries.filter(candidate => candidate !== entry)
        rebuild()
    }

    function rebuild() {
        const live = entries.filter(entry => entry.item && entry.item.visible
            && entry.item.width > 0)
        live.sort((a, b) => {
            const ai = keyIndex(a.key)
            const bi = keyIndex(b.key)
            return ai === bi ? a.key.localeCompare(b.key) : ai - bi
        })
        const membershipChanged = live.length !== orderedEntries.length
            || live.some(entry => !orderedEntries.includes(entry))
        if (dragActive && membershipChanged) {
            draggedItem = null
            dragMoved = false
            previewEntries = live.slice(0)
        }
        orderedEntries = live
        if (!dragActive)
            previewEntries = live.slice(0)
        positionEntries()
    }

    function positionEntries() {
        const items = dragActive ? previewEntries : orderedEntries
        let x = 0
        for (let i = 0; i < items.length; ++i) {
            const entry = items[i]
            if (!entry.item)
                continue
            entry.item.x = x
            entry.item.y = Math.round((height - entry.item.height) / 2)
            entry.item.z = entry === draggedItem ? 2 : 0
            x += entry.item.width + itemSpacing
        }
        implicitWidth = Math.max(0, x - (items.length ? itemSpacing : 0))
        implicitHeight = items.reduce((h, entry) => Math.max(h, entry.item ? entry.item.height : 0), 0)
    }

    function beginDrag(item, key) {
        const entry = entries.find(candidate => candidate.item === item
            && (!key || candidate.key === key))
        if (!entry || draggedItem)
            return false
        draggedItem = entry
        dragMoved = false
        previewEntries = orderedEntries.slice(0)
        dragPointerX = item.x + item.width / 2
        positionEntries()
        if (popupController && popupController.surfaceRequired)
            popupController.close()
        return true
    }

    function updateDrag(item, key, sceneX) {
        if (!draggedItem || draggedItem.item !== item || (key && draggedItem.key !== key))
            return false
        dragPointerX = mapFromItem(null, sceneX, 0).x
        const others = previewEntries.filter(entry => entry !== draggedItem)
        let targetIndex = 0
        for (const entry of others) {
            const center = entry.item.x + entry.item.width / 2
            if (dragPointerX > center)
                ++targetIndex
        }
        targetIndex = Math.max(0, Math.min(others.length, targetIndex))
        const next = others.slice(0)
        next.splice(targetIndex, 0, draggedItem)
        if (next.some((entry, index) => previewEntries[index] !== entry)) {
            previewEntries = next
            dragMoved = true
        }
        positionEntries()
        return true
    }

    function endDrag(item, key, commit) {
        if (!draggedItem || draggedItem.item !== item || (key && draggedItem.key !== key))
            return false
        const candidate = previewEntries.map(entry => entry.key)
        const original = orderedEntries.map(entry => entry.key)
        const orderChanged = candidate.length !== original.length
            || candidate.some((key, index) => key !== original[index])
        let committed = false
        if (commit && dragMoved && orderChanged) {
            if (orderStore && orderStore.commitVisibleOrder)
                committed = orderStore.commitVisibleOrder(candidate)
            else {
                const visibleSet = new Set(orderedEntries.map(entry => entry.key))
                const positions = []
                for (let i = 0; i < localOrder.length; ++i) {
                    if (visibleSet.has(localOrder[i]))
                        positions.push(i)
                }
                for (let i = 0; i < positions.length; ++i)
                    localOrder[positions[i]] = candidate[i]
                committed = true
            }
        }
        draggedItem = null
        dragMoved = false
        rebuild()
        return committed
    }

    onOrderStoreChanged: rebuild()
    Connections {
        target: layout.orderStore
        function onOrderChanged() { layout.rebuild() }
    }
    Item { id: itemHost; anchors.fill: parent }
}
