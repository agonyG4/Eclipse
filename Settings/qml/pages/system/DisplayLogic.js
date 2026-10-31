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

function outputWidth(output) { return previewSize(output, false).width }

function outputHeight(output) { return previewSize(output, false).height }

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

function boundsForOutputs(outputs, reference) {
    if (!outputs || outputs.length === 0)
        return { minX: 0, minY: 0, width: 1920, height: 1080 }
    let minX = Number(outputs[0].x) || 0
    let minY = Number(outputs[0].y) || 0
    let maxX = minX + previewSize(outputs[0], reference).width
    let maxY = minY + previewSize(outputs[0], reference).height
    for (let index = 1; index < outputs.length; ++index) {
        const output = outputs[index]
        const x = Number(output.x) || 0
        const y = Number(output.y) || 0
        minX = Math.min(minX, x)
        minY = Math.min(minY, y)
        maxX = Math.max(maxX, x + previewSize(output, reference).width)
        maxY = Math.max(maxY, y + previewSize(output, reference).height)
    }
    return { minX, minY, width: Math.max(1, maxX - minX), height: Math.max(1, maxY - minY) }
}
