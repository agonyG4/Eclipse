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
    property real dragGrabOffset: 0
    property bool dragMoved: false
    // Neighbours glide to their new slots while dragging and the dropped item
    // settles into place; ordinary relayouts (clock ticks, tray churn) snap.
    property int slideDuration: 220
    property bool settling: false
    readonly property real liftScale: 1.12
    readonly property real liftOpacity: 0.85

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
        if (entry.slide)
            entry.slide.destroy()
        if (entry.lift)
            entry.lift.destroy()
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

    function moveItem(entry, x) {
        const animate = (dragActive || settling) && slideDuration > 0
        if (!entry.slide && animate)
            entry.slide = slideComponent.createObject(layout, { target: entry.item })
        if (!animate) {
            if (entry.slide)
                entry.slide.stop()
            entry.item.x = x
            return
        }
        if (entry.slide.running && entry.slide.to === x)
            return
        if (!entry.slide.running && entry.item.x === x)
            return
        entry.slide.stop()
        entry.slide.to = x
        entry.slide.start()
    }

    function setLifted(entry, lifted) {
        if (!entry || !entry.item)
            return
        if (!entry.lift)
            entry.lift = liftComponent.createObject(layout, { target: entry.item })
        entry.lift.stop()
        entry.lift.scaleTo = lifted ? liftScale : 1
        entry.lift.opacityTo = lifted ? liftOpacity : 1
        entry.lift.start()
    }

    function followPointer() {
        if (!draggedItem || !draggedItem.item || !dragMoved)
            return
        const item = draggedItem.item
        if (draggedItem.slide)
            draggedItem.slide.stop()
        const maxX = Math.max(0, implicitWidth - item.width)
        item.x = Math.max(0, Math.min(maxX, dragPointerX - dragGrabOffset))
    }

    function positionEntries() {
        const items = dragActive ? previewEntries : orderedEntries
        let x = 0
        for (let i = 0; i < items.length; ++i) {
            const entry = items[i]
            if (!entry.item)
                continue
            entry.slotX = x
            entry.item.y = Math.round((height - entry.item.height) / 2)
            entry.item.z = entry === draggedItem ? 2 : 0
            if (entry !== draggedItem || !dragMoved)
                moveItem(entry, x)
            x += entry.item.width + itemSpacing
        }
        implicitWidth = Math.max(0, x - (items.length ? itemSpacing : 0))
        implicitHeight = items.reduce((h, entry) => Math.max(h, entry.item ? entry.item.height : 0), 0)
        followPointer()
    }

    function beginDrag(item, key, sceneX) {
        const entry = entries.find(candidate => candidate.item === item
            && (!key || candidate.key === key))
        if (!entry || draggedItem)
            return false
        draggedItem = entry
        dragMoved = false
        previewEntries = orderedEntries.slice(0)
        dragPointerX = sceneX === undefined ? item.x + item.width / 2
                                            : mapFromItem(null, sceneX, 0).x
        dragGrabOffset = Math.max(0, Math.min(item.width, dragPointerX - item.x))
        setLifted(entry, true)
        positionEntries()
        if (popupController && popupController.surfaceRequired)
            popupController.close()
        return true
    }

    function updateDrag(item, key, sceneX) {
        if (!draggedItem || draggedItem.item !== item || (key && draggedItem.key !== key))
            return false
        // The lifted item is scaled, so mapped pointer positions carry float
        // noise; round it so a pointer resting on a neighbour's centre is a
        // stable tie instead of flipping between slots.
        dragPointerX = Math.round(mapFromItem(null, sceneX, 0).x * 100) / 100
        const others = previewEntries.filter(entry => entry !== draggedItem)
        let targetIndex = 0
        for (const entry of others) {
            // Compare against resting slots, not mid-glide positions.
            const slot = entry.slotX === undefined ? entry.item.x : entry.slotX
            const center = slot + entry.item.width / 2
            if (dragPointerX > center)
                ++targetIndex
        }
        targetIndex = Math.max(0, Math.min(others.length, targetIndex))
        const next = others.slice(0)
        next.splice(targetIndex, 0, draggedItem)
        if (next.some((entry, index) => previewEntries[index] !== entry))
            previewEntries = next
        dragMoved = true
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
        const dropped = draggedItem
        settling = true
        settleTimer.restart()
        draggedItem = null
        dragMoved = false
        setLifted(dropped, false)
        rebuild()
        return committed
    }

    Timer {
        id: settleTimer
        interval: layout.slideDuration + 40
        onTriggered: layout.settling = false
    }

    Component {
        id: slideComponent
        NumberAnimation {
            property: "x"
            duration: layout.slideDuration
            easing.type: Easing.OutCubic
        }
    }

    Component {
        id: liftComponent
        ParallelAnimation {
            id: lift
            property Item target: null
            property real scaleTo: 1
            property real opacityTo: 1
            NumberAnimation {
                target: lift.target
                property: "scale"
                to: lift.scaleTo
                duration: 160
                easing.type: Easing.OutBack
                easing.overshoot: 2
            }
            NumberAnimation {
                target: lift.target
                property: "opacity"
                to: lift.opacityTo
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
    }

    onOrderStoreChanged: rebuild()
    Connections {
        target: layout.orderStore
        function onOrderChanged() { layout.rebuild() }
    }
    Item { id: itemHost; anchors.fill: parent }
}
