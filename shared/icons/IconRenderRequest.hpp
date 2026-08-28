#pragma once

#include <QtGlobal>

#include <cmath>
#include <limits>
#include <optional>

struct IconRenderRequest final {
    static constexpr int kMaxLogicalExtent = 256;
    static constexpr int kMaxPhysicalExtent = 1024;
    static constexpr qreal kMinDevicePixelRatio = 0.25;
    static constexpr qreal kMaxDevicePixelRatio = 4.0;
    static constexpr int kDprPrecision = 1000;

    int logicalExtent = 1;
    qreal devicePixelRatio = 1.0;
    int physicalExtent = 1;

    static std::optional<IconRenderRequest> fromValues(qreal logical, qreal dpr)
    {
        if (!std::isfinite(logical) || !std::isfinite(dpr) || logical <= 0.0 || dpr <= 0.0)
            return std::nullopt;

        const qreal roundedLogical = std::ceil(logical);
        if (!std::isfinite(roundedLogical))
            return std::nullopt;

        const int logicalExtent = qBound(1, static_cast<int>(qMin(
            roundedLogical, static_cast<qreal>(kMaxLogicalExtent))), kMaxLogicalExtent);
        const qreal normalizedDpr = quantize(qBound(kMinDevicePixelRatio,
                                                     dpr,
                                                     kMaxDevicePixelRatio));
        const qreal product = static_cast<qreal>(logicalExtent) * normalizedDpr;
        if (!std::isfinite(product))
            return std::nullopt;

        const qreal roundedPhysical = std::ceil(product);
        if (!std::isfinite(roundedPhysical))
            return std::nullopt;

        IconRenderRequest request;
        request.logicalExtent = logicalExtent;
        request.devicePixelRatio = normalizedDpr;
        request.physicalExtent = qBound(1, static_cast<int>(qMin(
            roundedPhysical, static_cast<qreal>(kMaxPhysicalExtent))), kMaxPhysicalExtent);
        return request;
    }

private:
    static qreal quantize(qreal value)
    {
        return std::round(value * static_cast<qreal>(kDprPrecision))
            / static_cast<qreal>(kDprPrecision);
    }
};
