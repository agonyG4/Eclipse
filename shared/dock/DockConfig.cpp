#include "dock/DockConfig.hpp"

#include <QChar>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtMath>

namespace {

bool stringIn(const QString &value, std::initializer_list<QLatin1StringView> allowed)
{
    for (const QLatin1StringView candidate : allowed) {
        if (value == candidate)
            return true;
    }
    return false;
}

QString stringField(const QJsonObject &object, const QString &key, const QString &fallback,
                    std::initializer_list<QLatin1StringView> allowed, QStringList *errors)
{
    if (!object.contains(key))
        return fallback;
    const QJsonValue value = object.value(key);
    if (!value.isString() || !stringIn(value.toString(), allowed)) {
        if (errors)
            errors->append(QStringLiteral("%1 has an unsupported value").arg(key));
        return fallback;
    }
    return value.toString();
}

int integerField(const QJsonObject &object, const QString &key, int fallback,
                 int minimum, int maximum, QStringList *errors)
{
    if (!object.contains(key))
        return fallback;
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        if (errors)
            errors->append(QStringLiteral("%1 must be numeric").arg(key));
        return fallback;
    }
    const double number = value.toDouble();
    if (!qIsFinite(number)) {
        if (errors)
            errors->append(QStringLiteral("%1 must be finite").arg(key));
        return fallback;
    }
    return qRound(qBound(static_cast<double>(minimum), number,
                         static_cast<double>(maximum)));
}

double doubleField(const QJsonObject &object, const QString &key, double fallback,
                   double minimum, double maximum, QStringList *errors)
{
    if (!object.contains(key))
        return fallback;
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        if (errors)
            errors->append(QStringLiteral("%1 must be numeric").arg(key));
        return fallback;
    }
    const double number = value.toDouble();
    if (!qIsFinite(number)) {
        if (errors)
            errors->append(QStringLiteral("%1 must be finite").arg(key));
        return fallback;
    }
    return qBound(minimum, number, maximum);
}

bool booleanField(const QJsonObject &object, const QString &key, bool fallback,
                  QStringList *errors)
{
    if (!object.contains(key))
        return fallback;
    const QJsonValue value = object.value(key);
    if (!value.isBool()) {
        if (errors)
            errors->append(QStringLiteral("%1 must be boolean").arg(key));
        return fallback;
    }
    return value.toBool();
}

} // namespace

DockConfig DockConfig::defaults()
{
    return {};
}

namespace DockConfigCodec {

JsonResult readJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {{}, true, QStringLiteral("Cannot read %1").arg(path)};
    if (file.size() > kMaximumConfigBytes) {
        return {{}, true, QStringLiteral("Configuration file is too large: %1").arg(path)};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        return {{}, true,
                QStringLiteral("Invalid JSON in %1: %2").arg(path, parseError.errorString())};
    }
    return {document.object(), true, {}};
}

DockConfig parse(const QJsonObject &object, QStringList *errors)
{
    QStringList localErrors;
    QStringList *diagnostics = errors ? errors : &localErrors;
    DockConfig result = DockConfig::defaults();
    result.iconSize = integerField(object, QStringLiteral("iconSize"), result.iconSize,
                                   32, 64, diagnostics);
    result.panelPadding = integerField(object, QStringLiteral("panelPadding"),
                                       result.panelPadding, 8, 32, diagnostics);
    result.itemSpacing = integerField(object, QStringLiteral("itemSpacing"),
                                      result.itemSpacing, 4, 24, diagnostics);
    result.hoverEffect = stringField(
        object, QStringLiteral("hoverEffect"), result.hoverEffect,
        {QLatin1StringView("none"), QLatin1StringView("lift"),
         QLatin1StringView("magnification")}, diagnostics);
    if (!object.contains(QStringLiteral("hoverEffect"))
        && object.contains(QStringLiteral("magnificationEnabled"))) {
        const bool enabled = booleanField(object, QStringLiteral("magnificationEnabled"),
                                          true, diagnostics);
        result.hoverEffect = enabled ? QStringLiteral("magnification")
                                     : QStringLiteral("none");
    }
    result.magnificationScale = doubleField(object, QStringLiteral("magnificationScale"),
                                            result.magnificationScale, 1.0, 2.0, diagnostics);
    result.magnificationRadius = doubleField(object, QStringLiteral("magnificationRadius"),
                                             result.magnificationRadius, 1.0, 4.0, diagnostics);

    const QString edgeKey = object.contains(QStringLiteral("edgeMargin"))
        ? QStringLiteral("edgeMargin") : QStringLiteral("bottomMargin");
    result.edgeMargin = integerField(object, edgeKey, result.edgeMargin, 0, 48, diagnostics);
    result.bottomMargin = result.edgeMargin;
    result.position = stringField(
        object, QStringLiteral("position"), result.position,
        {QLatin1StringView("bottom"), QLatin1StringView("left"), QLatin1StringView("right")},
        diagnostics);
    result.floating = booleanField(object, QStringLiteral("floating"), result.floating, diagnostics);
    result.cornerRadius = integerField(object, QStringLiteral("cornerRadius"),
                                       result.cornerRadius, 0, 48, diagnostics);
    result.autoHide = stringField(
        object, QStringLiteral("autoHide"), result.autoHide,
        {QLatin1StringView("never"), QLatin1StringView("intelligent"),
         QLatin1StringView("always")}, diagnostics);
    result.indicatorStyle = stringField(
        object, QStringLiteral("indicatorStyle"), result.indicatorStyle,
        {QLatin1StringView("line"), QLatin1StringView("dot"), QLatin1StringView("none")},
        diagnostics);
    result.indicatorSize = integerField(object, QStringLiteral("indicatorSize"),
                                         result.indicatorSize, 1, 12, diagnostics);
    result.animationsEnabled = booleanField(object, QStringLiteral("animationsEnabled"),
                                             result.animationsEnabled, diagnostics);
    result.animationSpeed = doubleField(object, QStringLiteral("animationSpeed"),
                                         result.animationSpeed, 0.25, 4.0, diagnostics);

    if (!object.contains(QStringLiteral("pins")))
        return result;
    const QJsonValue pinsValue = object.value(QStringLiteral("pins"));
    if (!pinsValue.isArray()) {
        diagnostics->append(QStringLiteral("pins must be an array"));
        return result;
    }
    const QJsonArray pins = pinsValue.toArray();
    if (pins.size() > kMaximumPins) {
        diagnostics->append(QStringLiteral("pins contains too many entries"));
        return result;
    }
    for (const QJsonValue &value : pins) {
        if (!value.isString()) {
            diagnostics->append(QStringLiteral("pins entries must be strings"));
            continue;
        }
        const QString pin = value.toString();
        if (!validDesktopFileName(pin)) {
            diagnostics->append(QStringLiteral("invalid desktop filename in pins: %1").arg(pin));
            continue;
        }
        if (!result.pins.contains(pin))
            result.pins.append(pin);
    }
    return result;
}

bool validDesktopFileName(const QString &fileName)
{
    return !fileName.isEmpty() && fileName.size() <= kMaximumPinLength
        && fileName.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)
        && !fileName.contains(QLatin1Char('/'))
        && !fileName.contains(QLatin1Char('\\'))
        && !fileName.contains(QChar::Null)
        && !fileName.contains(QStringLiteral(".."));
}

bool validatePinList(const QStringList &pins, QString *errorOut)
{
    if (pins.size() > kMaximumPins) {
        if (errorOut)
            *errorOut = QStringLiteral("pins contains too many entries");
        return false;
    }
    QStringList seen;
    seen.reserve(pins.size());
    for (const QString &pin : pins) {
        if (!validDesktopFileName(pin)) {
            if (errorOut)
                *errorOut = QStringLiteral("invalid desktop filename in pins: %1").arg(pin);
            return false;
        }
        if (seen.contains(pin)) {
            if (errorOut)
                *errorOut = QStringLiteral("duplicate desktop filename in pins: %1").arg(pin);
            return false;
        }
        seen.append(pin);
    }
    return true;
}

bool validateConfig(const DockConfig &config, QString *errorOut)
{
    const auto fail = [errorOut](const QString &error) {
        if (errorOut)
            *errorOut = error;
        return false;
    };
    if (config.iconSize < 32 || config.iconSize > 64)
        return fail(QStringLiteral("iconSize is outside 32..64"));
    if (config.panelPadding < 8 || config.panelPadding > 32)
        return fail(QStringLiteral("panelPadding is outside 8..32"));
    if (config.itemSpacing < 4 || config.itemSpacing > 24)
        return fail(QStringLiteral("itemSpacing is outside 4..24"));
    if (!stringIn(config.hoverEffect, {QLatin1StringView("none"), QLatin1StringView("lift"),
                                       QLatin1StringView("magnification")}))
        return fail(QStringLiteral("hoverEffect is unsupported"));
    if (!qIsFinite(config.magnificationScale) || config.magnificationScale < 1.0
        || config.magnificationScale > 2.0)
        return fail(QStringLiteral("magnificationScale is outside 1.0..2.0"));
    if (!qIsFinite(config.magnificationRadius) || config.magnificationRadius < 1.0
        || config.magnificationRadius > 4.0)
        return fail(QStringLiteral("magnificationRadius is outside 1.0..4.0"));
    if (config.edgeMargin < 0 || config.edgeMargin > 48)
        return fail(QStringLiteral("edgeMargin is outside 0..48"));
    if (!stringIn(config.position, {QLatin1StringView("bottom"), QLatin1StringView("left"),
                                    QLatin1StringView("right")}))
        return fail(QStringLiteral("position is unsupported"));
    if (config.cornerRadius < 0 || config.cornerRadius > 48)
        return fail(QStringLiteral("cornerRadius is outside 0..48"));
    if (!stringIn(config.autoHide, {QLatin1StringView("never"), QLatin1StringView("intelligent"),
                                    QLatin1StringView("always")}))
        return fail(QStringLiteral("autoHide is unsupported"));
    if (!stringIn(config.indicatorStyle, {QLatin1StringView("line"), QLatin1StringView("dot"),
                                          QLatin1StringView("none")}))
        return fail(QStringLiteral("indicatorStyle is unsupported"));
    if (config.indicatorSize < 1 || config.indicatorSize > 12)
        return fail(QStringLiteral("indicatorSize is outside 1..12"));
    if (!qIsFinite(config.animationSpeed) || config.animationSpeed < 0.25
        || config.animationSpeed > 4.0)
        return fail(QStringLiteral("animationSpeed is outside 0.25..4.0"));
    return validatePinList(config.pins, errorOut);
}

QJsonObject patchKnownFields(const QJsonObject &source, const DockConfig &config,
                             bool includePins)
{
    QJsonObject object = source;
    object.insert(QStringLiteral("iconSize"), config.iconSize);
    object.insert(QStringLiteral("panelPadding"), config.panelPadding);
    object.insert(QStringLiteral("itemSpacing"), config.itemSpacing);
    object.insert(QStringLiteral("hoverEffect"), config.hoverEffect);
    object.insert(QStringLiteral("magnificationScale"), config.magnificationScale);
    object.insert(QStringLiteral("magnificationRadius"), config.magnificationRadius);
    object.insert(QStringLiteral("edgeMargin"), config.edgeMargin);
    object.remove(QStringLiteral("bottomMargin"));
    object.insert(QStringLiteral("position"), config.position);
    object.insert(QStringLiteral("floating"), config.floating);
    object.insert(QStringLiteral("cornerRadius"), config.cornerRadius);
    object.insert(QStringLiteral("autoHide"), config.autoHide);
    object.insert(QStringLiteral("indicatorStyle"), config.indicatorStyle);
    object.insert(QStringLiteral("indicatorSize"), config.indicatorSize);
    object.insert(QStringLiteral("animationsEnabled"), config.animationsEnabled);
    object.insert(QStringLiteral("animationSpeed"), config.animationSpeed);
    if (includePins) {
        QJsonArray pins;
        for (const QString &pin : config.pins)
            pins.append(pin);
        object.insert(QStringLiteral("pins"), pins);
    }
    return object;
}

} // namespace DockConfigCodec
