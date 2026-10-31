// Pure display naming, geometry, mode selection, and formatting helpers.
.pragma library

function nonEmpty(value) {
    return value !== undefined && value !== null
        && String(value).trim().length > 0
}

function isBuiltIn(output) {
    const name = output && nonEmpty(output.name)
        ? String(output.name).toLowerCase() : ""
    return name.startsWith("edp") || name.startsWith("lvds")
}

function titleFor(output) {
    if (!output)
        return "Display"
    if (nonEmpty(output.model))
        return String(output.model).trim()
    if (nonEmpty(output.make))
        return String(output.make).trim()
    if (nonEmpty(output.name))
        return String(output.name)
    return isBuiltIn(output) ? "Laptop Display" : "Display"
}

function kindFor(output) {
    return isBuiltIn(output) ? "Built-in display" : "External display"
}

function subtitleFor(output) {
    if (!output)
        return ""
    const pieces = [kindFor(output)]
    const make = nonEmpty(output.make) ? String(output.make).trim() : ""
    if (make !== "" && make !== titleFor(output))
        pieces.push(make)
    if (nonEmpty(output.name))
        pieces.push(String(output.name))
    return pieces.join(" · ")
}

function monitorSelectorLabel(output) {
    if (!output)
        return "Display"
    const title = titleFor(output)
    const connector = nonEmpty(output.name) ? String(output.name) : ""
    return connector !== "" && connector !== title
        ? title + " · " + connector : title
}

function isQuarterTurn(output) {
    const transform = selectedTransform(output)
    return transform === "90" || transform === "270"
        || transform === "flipped_90" || transform === "flipped_270"
}

function previewSize(output, reference) {
    const mode = selectedMode(output)
    let width = mode ? Number(mode.width) : Number(output && output.currentWidth)
    let height = mode ? Number(mode.height) : Number(output && output.currentHeight)
    if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) {
        width = 240
        height = 135
    }
    // Keep a stable reference while selecting smaller modes of the same aspect.
    if (reference && output && output.modes) {
        for (const candidate of output.modes) {
            if (candidate.preferred || String(candidate.id) === String(output.currentModeId)) {
                width = Math.max(width, Number(candidate.width) || 0)
                height = Math.max(height, Number(candidate.height) || 0)
            }
        }
    }
    return isQuarterTurn(output) ? { width: height, height: width } : { width, height }
}

const MM_PER_PIXEL_AT_96_DPI = 25.4 / 96

// Device chrome around the glass: bezel width and how far the stand or deck
// reaches below the panel.
function previewChrome(output) {
    return isBuiltIn(output) ? { bezel: 12, foot: 11 } : { bezel: 10, foot: 26 }
}

// Physical diagonal in millimetres. Displays that report no size fall back to
// their largest native mode at 96 DPI.
function previewDiagonalMm(output) {
    const width = Number(output && output.physicalWidthMm)
    const height = Number(output && output.physicalHeightMm)
    if (output && Boolean(output.hasPhysicalSize) && width > 0 && height > 0)
        return Math.hypot(width, height)
    const native = previewSize(output, true)
    return Math.hypot(native.width, native.height) * MM_PER_PIXEL_AT_96_DPI
}

function sizeForDiagonal(diagonal, aspect) {
    const length = Math.hypot(aspect.width, aspect.height)
    return { width: diagonal * aspect.width / length, height: diagonal * aspect.height / length }
}

// Lays displays out as a row of devices, the way macOS shows them: physical
// diagonal sets the size, the selected mode and rotation set the shape, and
// desktop position only sets the order. All devices share one scale and stand
// on a common baseline. Each slot is sized for both the selected and the
// native shape so picking a mode does not shuffle the row.
// `label` reserves a common strip under every device for its name.
function previewLayout(outputs, width, height, gap, padding, label) {
    const nameStrip = Math.max(0, Number(label) || 0)
    const list = outputs || []
    if (list.length === 0)
        return []
    const entries = list.map(output => {
        const diagonal = previewDiagonalMm(output)
        const selected = sizeForDiagonal(diagonal, previewSize(output, false))
        const native = sizeForDiagonal(diagonal, previewSize(output, true))
        const chrome = previewChrome(output)
        return {
            selected,
            bezel: chrome.bezel,
            below: Math.max(chrome.bezel, chrome.foot) + nameStrip,
            boxWidth: Math.max(selected.width, native.width),
            boxHeight: Math.max(selected.height, native.height)
        }
    })
    let chromeWidth = gap * (list.length - 1)
    let boxWidth = 0
    for (const entry of entries) {
        chromeWidth += entry.bezel * 2
        boxWidth += entry.boxWidth
    }
    let scale = (width - padding * 2 - chromeWidth) / Math.max(1, boxWidth)
    for (const entry of entries)
        scale = Math.min(scale, (height - padding * 2 - entry.bezel - entry.below)
            / Math.max(1, entry.boxHeight))
    scale = Math.max(0.001, scale)

    let rowHeight = 0
    for (const entry of entries)
        rowHeight = Math.max(rowHeight, entry.bezel + entry.boxHeight * scale + entry.below)
    const baseline = (height + rowHeight) / 2
    const order = list.map((output, index) => index).sort((a, b) => {
        const dx = (Number(list[a].x) || 0) - (Number(list[b].x) || 0)
        return dx !== 0 ? dx : (Number(list[a].y) || 0) - (Number(list[b].y) || 0)
    })
    const result = new Array(list.length)
    let cursor = (width - chromeWidth - boxWidth * scale) / 2
    for (const index of order) {
        const entry = entries[index]
        const slotWidth = entry.boxWidth * scale + entry.bezel * 2
        const screenWidth = entry.selected.width * scale
        const screenHeight = entry.selected.height * scale
        result[index] = {
            x: cursor + (slotWidth - screenWidth - entry.bezel * 2) / 2,
            y: baseline - entry.below - screenHeight - entry.bezel,
            screenWidth,
            screenHeight,
            bezel: entry.bezel,
            below: entry.below
        }
        cursor += slotWidth + gap
    }
    return result
}

function resolutionKey(group) {
    return String(group.width) + "x" + String(group.height)
}

function formatResolution(group) {
    return String(group.width) + " × " + String(group.height)
}

function formatRate(milliHz) {
    const hz = Number(milliHz) / 1000
    if (!Number.isFinite(hz) || hz <= 0)
        return "Unavailable"
    // Mode timings land just off the nominal rate (164.999 for 165 Hz);
    // show the nominal rate, but keep real fractional rates like 59.94.
    const nominal = Math.round(hz)
    if (Math.abs(hz - nominal) < 0.015)
        return String(nominal)
    return String(Number(hz.toFixed(2)))
}

function formatScale(scaleMilli) {
    const percent = Number(scaleMilli) / 10
    if (!Number.isFinite(percent) || percent <= 0)
        return "Unavailable"
    return (Number.isInteger(percent)
        ? String(percent) : String(Number(percent.toFixed(1)))) + "%"
}

function currentMode(output) {
    if (!output || !Boolean(output.hasCurrentMode) || !output.modes)
        return null
    return modeById(output, output.currentModeId)
}

function modeById(output, modeId) {
    if (!output || !output.modes || modeId === undefined || modeId === null)
        return null
    const requestedId = String(modeId)
    for (let index = 0; index < output.modes.length; ++index) {
        if (String(output.modes[index].id) === requestedId)
            return output.modes[index]
    }
    return null
}

function selectedMode(output) {
    if (!output)
        return null
    const draft = Boolean(output.draftPresent)
        ? modeById(output, output.draftModeId) : null
    return draft || currentMode(output)
}

function selectedScaleMilli(output) {
    if (!output)
        return 0
    const draftScale = Number(output.draftScaleMilli)
    return Boolean(output.draftPresent) && draftScale > 0
        ? draftScale : Number(output.scaleMilli)
}

function selectedTransform(output) {
    if (!output)
        return "normal"
    return Boolean(output.draftPresent)
        ? String(output.draftTransform) : String(output.transform)
}

function rateText(mode) {
    if (!mode)
        return "Unavailable"
    const rate = formatRate(mode.refreshMilliHz)
    return rate === "Unavailable" ? rate
        : rate + " Hz" + (Boolean(mode.interlaced) ? " (interlaced)" : "")
}

function refreshLabel(output) {
    if (!output || !Boolean(output.hasCurrentMode))
        return "Unavailable"
    const rate = formatRate(output.currentRefreshMilliHz)
    if (rate === "Unavailable")
        return rate
    const mode = currentMode(output)
    return rate + " Hz" + (mode && mode.interlaced ? " (interlaced)" : "")
}

function selectedRefreshLabel(output) {
    if (!output)
        return "Unavailable"
    const mode = selectedMode(output)
    return mode ? rateText(mode) : refreshLabel(output)
}

function modeSummary(output) {
    if (!output || !Boolean(output.hasCurrentMode))
        return "No current mode"
    return String(output.currentWidth) + " × " + String(output.currentHeight)
        + " • " + refreshLabel(output)
}

function vrrText(state) {
    switch (String(state)) {
    case "active": return "Active"
    case "available":
    case "configured": return "Supported"
    case "unavailable": return "Not supported"
    case "degraded": return "Unavailable"
    default: return "Unavailable"
    }
}

function resolutionGroups(output) {
    if (!output || !output.modes)
        return []

    const selectionMode = selectedMode(output)
    const selectionWidth = selectionMode
        ? Number(selectionMode.width) : Number(output.currentWidth)
    const selectionHeight = selectionMode
        ? Number(selectionMode.height) : Number(output.currentHeight)

    const bySize = Object.create(null)
    for (let index = 0; index < output.modes.length; ++index) {
        const mode = output.modes[index]
        const width = Number(mode.width)
        const height = Number(mode.height)
        if (width <= 0 || height <= 0)
            continue
        const key = String(width) + "x" + String(height)
        if (!bySize[key]) {
            bySize[key] = {
                width: width,
                height: height,
                preferred: false,
                current: (selectionMode !== null || Boolean(output.hasCurrentMode))
                    && selectionWidth === width
                    && selectionHeight === height,
                rates: []
            }
        }
        const group = bySize[key]
        group.preferred = group.preferred || Boolean(mode.preferred)
        group.rates.push({
            id: mode.id,
            refreshMilliHz: Number(mode.refreshMilliHz),
            interlaced: Boolean(mode.interlaced),
            preferred: Boolean(mode.preferred)
        })
    }

    const groups = Object.keys(bySize).map(key => bySize[key])
    if (Boolean(output.hasCurrentMode)) {
        const currentKey = String(Number(output.currentWidth)) + "x"
            + String(Number(output.currentHeight))
        if (!bySize[currentKey] && Number(output.currentWidth) > 0
                && Number(output.currentHeight) > 0) {
            groups.push({
                width: Number(output.currentWidth),
                height: Number(output.currentHeight),
                preferred: false,
                current: true,
                rates: []
            })
        }
    }

    groups.sort((left, right) =>
        right.width * right.height - left.width * left.height
            || right.width - left.width || right.height - left.height)
    for (let groupIndex = 0; groupIndex < groups.length; ++groupIndex) {
        groups[groupIndex].rates.sort((left, right) =>
            left.refreshMilliHz - right.refreshMilliHz
                || Number(left.interlaced) - Number(right.interlaced)
                || Number(left.id) - Number(right.id))
    }
    return groups
}

function modeForResolution(group, output) {
    if (!group || !group.rates || group.rates.length === 0)
        return null

    const current = selectedMode(output)
    if (current) {
        const matching = group.rates.filter(mode =>
            Number(mode.refreshMilliHz) === Number(current.refreshMilliHz)
                && Boolean(mode.interlaced) === Boolean(current.interlaced))
        if (matching.length > 0)
            return matching.sort((left, right) =>
                Number(left.id) - Number(right.id))[0]
    }

    const preferred = group.rates.filter(mode => Boolean(mode.preferred))
    const candidates = preferred.length > 0 ? preferred : group.rates
    const preservingInterlace = current
        ? candidates.filter(mode =>
            Boolean(mode.interlaced) === Boolean(current.interlaced)) : []
    const ranked = (preservingInterlace.length > 0
        ? preservingInterlace : candidates).slice().sort((left, right) =>
            Number(right.refreshMilliHz) - Number(left.refreshMilliHz)
                || Number(left.interlaced) - Number(right.interlaced)
                || Number(left.id) - Number(right.id))
    return ranked.length > 0 ? ranked[0] : null
}

function selectedRefreshIndex(output, group) {
    if (!output || !group || !group.rates)
        return -1
    const selected = selectedMode(output)
    if (!selected)
        return -1
    return group.rates.findIndex(mode => String(mode.id) === String(selected.id))
}

function colorDepthText(bits) {
    const value = Number(bits)
    return value > 0 ? String(value) + "-bit" : "Unavailable"
}

function colorDepthOptions(output) {
    if (!output || !output.colorDepthOptions)
        return []
    return output.colorDepthOptions.map(value => Number(value))
}

function selectedColorDepthIndex(output) {
    return colorDepthOptions(output).indexOf(Number(output
        ? output.colorDepthBits : 0))
}

function scaleValues(output) {
    if (!output || !output.scaleValuesMilli)
        return []
    return output.scaleValuesMilli.map(value => Number(value))
}

function scaleLabels(output) {
    return scaleValues(output).map(value => formatScale(value))
}

function selectedScaleIndex(output) {
    const values = scaleValues(output)
    const selected = selectedScaleMilli(output)
    return values.findIndex(value => Number(value) === selected)
}

function defaultResolutionSubset(groups) {
    if (!groups || groups.length <= 5)
        return groups || []

    const selected = []
    const add = group => {
        if (group && selected.length < 5
                && !selected.some(item => resolutionKey(item)
                    === resolutionKey(group)))
            selected.push(group)
    }
    const current = groups.find(group => group.current)
    const preferred = groups.find(group => group.preferred)
    add(current)
    add(preferred)
    add(groups[0])

    const commonSizes = ["1600x900", "1366x768", "1280x720", "1024x768"]
    for (let index = 0; index < commonSizes.length && selected.length < 5; ++index)
        add(groups.find(group => resolutionKey(group) === commonSizes[index]))
    for (let index = 0; index < groups.length && selected.length < 5; ++index)
        add(groups[index])
    selected.sort((left, right) =>
        right.width * right.height - left.width * left.height
            || right.width - left.width || right.height - left.height)
    return selected
}

// SVG path for a rectangle with continuous (squircle) corners, the curve Apple
// uses instead of circular arcs. `inset` shrinks the outline so strokes stay inside.
function continuousRectPath(width, height, radius, inset) {
    const i = Math.max(0, Number(inset) || 0)
    const w = Number(width) - i * 2
    const h = Number(height) - i * 2
    if (!(w > 0) || !(h > 0))
        return ""
    const extent = 1.528665
    const r = Math.max(0, Math.min(Number(radius) - i, Math.min(w, h) / 2 / extent))
    // Corner outline in radius units: u runs back along the incoming edge,
    // v along the outgoing edge.
    const curve = [
        ["C", 1.088493, 0, 0.868407, 0, 0.669934, 0.065496],
        ["L", 0.631494, 0.074911],
        ["C", 0.372824, 0.169059, 0.169060, 0.372824, 0.074911, 0.631494],
        ["L", 0.065496, 0.669934],
        ["C", 0, 0.868407, 0, 1.088493, 0, extent]
    ]
    const corners = [
        { x: i + w, y: i, e1: [-1, 0], e2: [0, 1] },
        { x: i + w, y: i + h, e1: [0, -1], e2: [-1, 0] },
        { x: i, y: i + h, e1: [1, 0], e2: [0, -1] },
        { x: i, y: i, e1: [0, 1], e2: [1, 0] }
    ]
    const fmt = function (n) { return Number(n).toFixed(3) }
    const point = function (c, u, v) {
        return fmt(c.x + (u * c.e1[0] + v * c.e2[0]) * r) + " "
            + fmt(c.y + (u * c.e1[1] + v * c.e2[1]) * r)
    }
    let path = "M " + point(corners[3], 0, extent)
    for (const c of corners) {
        path += " L " + point(c, extent, 0)
        for (const seg of curve) {
            path += " " + seg[0]
            for (let k = 1; k < seg.length; k += 2)
                path += " " + point(c, seg[k], seg[k + 1])
        }
    }
    return path + " Z"
}
