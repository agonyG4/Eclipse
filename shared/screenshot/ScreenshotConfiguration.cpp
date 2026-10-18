#include "screenshot/ScreenshotConfiguration.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPalette>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

QString positionName(ScreenshotConfiguration::Position position)
{
    using Position = ScreenshotConfiguration::Position;
    switch (position) {
    case Position::BottomRight: return QStringLiteral("bottom-right");
    case Position::BottomLeft: return QStringLiteral("bottom-left");
    case Position::TopRight: return QStringLiteral("top-right");
    case Position::TopLeft: return QStringLiteral("top-left");
    }
    return QStringLiteral("bottom-right");
}

bool parsePosition(const QString &name, ScreenshotConfiguration::Position *position)
{
    using Position = ScreenshotConfiguration::Position;
    if (name == QStringLiteral("bottom-right")) *position = Position::BottomRight;
    else if (name == QStringLiteral("bottom-left")) *position = Position::BottomLeft;
    else if (name == QStringLiteral("top-right")) *position = Position::TopRight;
    else if (name == QStringLiteral("top-left")) *position = Position::TopLeft;
    else return false;
    return true;
}

QString animationName(ScreenshotConfiguration::AnimationStyle style)
{
    using Style = ScreenshotConfiguration::AnimationStyle;
    switch (style) {
    case Style::Fade: return QStringLiteral("fade");
    case Style::ScaleFade: return QStringLiteral("scale-fade");
    case Style::SlideFade: return QStringLiteral("slide-fade");
    }
    return QStringLiteral("scale-fade");
}

bool parseAnimation(const QString &name, ScreenshotConfiguration::AnimationStyle *style)
{
    using Style = ScreenshotConfiguration::AnimationStyle;
    if (name == QStringLiteral("fade")) *style = Style::Fade;
    else if (name == QStringLiteral("scale-fade")) *style = Style::ScaleFade;
    else if (name == QStringLiteral("slide-fade")) *style = Style::SlideFade;
    else return false;
    return true;
}

QString easingName(ScreenshotConfiguration::Easing easing)
{
    using Easing = ScreenshotConfiguration::Easing;
    switch (easing) {
    case Easing::Linear: return QStringLiteral("linear");
    case Easing::InQuad: return QStringLiteral("in-quad");
    case Easing::OutCubic: return QStringLiteral("out-cubic");
    case Easing::InOutCubic: return QStringLiteral("in-out-cubic");
    }
    return QStringLiteral("out-cubic");
}

bool parseEasing(const QString &name, ScreenshotConfiguration::Easing *easing)
{
    using Easing = ScreenshotConfiguration::Easing;
    if (name == QStringLiteral("linear")) *easing = Easing::Linear;
    else if (name == QStringLiteral("in-quad")) *easing = Easing::InQuad;
    else if (name == QStringLiteral("out-cubic")) *easing = Easing::OutCubic;
    else if (name == QStringLiteral("in-out-cubic")) *easing = Easing::InOutCubic;
    else return false;
    return true;
}

QString clickActionName(ScreenshotConfiguration::ClickAction action)
{
    using Action = ScreenshotConfiguration::ClickAction;
    switch (action) {
    case Action::Preview: return QStringLiteral("preview");
    case Action::Copy: return QStringLiteral("copy");
    case Action::Dismiss: return QStringLiteral("dismiss");
    }
    return QStringLiteral("preview");
}

bool parseClickAction(const QString &name, ScreenshotConfiguration::ClickAction *action)
{
    using Action = ScreenshotConfiguration::ClickAction;
    if (name == QStringLiteral("preview")) *action = Action::Preview;
    else if (name == QStringLiteral("copy")) *action = Action::Copy;
    else if (name == QStringLiteral("dismiss")) *action = Action::Dismiss;
    else return false;
    return true;
}

bool readInteger(const QJsonValue &value, int minimum, int maximum, int *out)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < minimum || number > maximum) {
        return false;
    }
    *out = static_cast<int>(number);
    return true;
}

bool readReal(const QJsonValue &value, qreal minimum, qreal maximum, qreal *out)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < minimum || number > maximum)
        return false;
    *out = static_cast<qreal>(number);
    return true;
}

bool readBoolean(const QJsonValue &value, bool *out)
{
    if (!value.isBool())
        return false;
    *out = value.toBool();
    return true;
}

bool readColor(const QJsonValue &value, QColor *out)
{
    if (!value.isString())
        return false;
    const QString text = value.toString();
    if (text.size() > 64)
        return false;
    const QColor color(text);
    if (!color.isValid())
        return false;
    *out = color;
    return true;
}

QJsonObject encodedValues(const ScreenshotConfiguration::Values &values)
{
    QJsonObject object;
    object.insert(QStringLiteral("cornerRadius"), values.cornerRadius);
    object.insert(QStringLiteral("borderEnabled"), values.borderEnabled);
    object.insert(QStringLiteral("borderWidth"), values.borderWidth);
    object.insert(QStringLiteral("borderColor"), values.borderColor.name(QColor::HexArgb));
    object.insert(QStringLiteral("shadowEnabled"), values.shadowEnabled);
    object.insert(QStringLiteral("shadowStrength"), values.shadowStrength);
    object.insert(QStringLiteral("thumbnailSize"), QJsonObject{
        {QStringLiteral("width"), values.thumbnailWidth},
        {QStringLiteral("height"), values.thumbnailHeight},
    });
    object.insert(QStringLiteral("previewMaximumScale"), values.previewMaximumScale);
    object.insert(QStringLiteral("position"), positionName(values.position));
    object.insert(QStringLiteral("edgeMargin"), values.edgeMargin);
    object.insert(QStringLiteral("animationsEnabled"), values.animationsEnabled);
    object.insert(QStringLiteral("entryAnimation"), animationName(values.entryAnimation));
    object.insert(QStringLiteral("exitAnimation"), animationName(values.exitAnimation));
    object.insert(QStringLiteral("entryDurationMs"), values.entryDurationMs);
    object.insert(QStringLiteral("exitDurationMs"), values.exitDurationMs);
    object.insert(QStringLiteral("entryEasing"), easingName(values.entryEasing));
    object.insert(QStringLiteral("exitEasing"), easingName(values.exitEasing));
    object.insert(QStringLiteral("showThumbnail"), values.showThumbnail);
    object.insert(QStringLiteral("autoCopy"), values.autoCopy);
    object.insert(QStringLiteral("dragEnabled"), values.dragEnabled);
    object.insert(QStringLiteral("displayDurationMs"), values.displayDurationMs);
    object.insert(QStringLiteral("clickAction"), clickActionName(values.clickAction));
    return object;
}

bool sameValues(const ScreenshotConfiguration::Values &left,
                const ScreenshotConfiguration::Values &right)
{
    return left.cornerRadius == right.cornerRadius
        && left.borderEnabled == right.borderEnabled
        && left.borderWidth == right.borderWidth
        && left.borderColor == right.borderColor
        && left.shadowEnabled == right.shadowEnabled
        && qFuzzyCompare(left.shadowStrength + 1.0, right.shadowStrength + 1.0)
        && left.thumbnailWidth == right.thumbnailWidth
        && left.thumbnailHeight == right.thumbnailHeight
        && qFuzzyCompare(left.previewMaximumScale + 1.0,
                         right.previewMaximumScale + 1.0)
        && left.position == right.position
        && left.edgeMargin == right.edgeMargin
        && left.animationsEnabled == right.animationsEnabled
        && left.entryAnimation == right.entryAnimation
        && left.exitAnimation == right.exitAnimation
        && left.entryDurationMs == right.entryDurationMs
        && left.exitDurationMs == right.exitDurationMs
        && left.entryEasing == right.entryEasing
        && left.exitEasing == right.exitEasing
        && left.showThumbnail == right.showThumbnail
        && left.autoCopy == right.autoCopy
        && left.dragEnabled == right.dragEnabled
        && left.displayDurationMs == right.displayDurationMs
        && left.clickAction == right.clickAction;
}

ScreenshotConfiguration::Values parsedValues(const QJsonObject &object,
                                              const ScreenshotConfiguration::Values &defaults,
                                              bool *hadInvalidValue)
{
    ScreenshotConfiguration::Values values = defaults;
    bool invalid = false;
    auto intField = [&object, &invalid](const QString &key, int minimum, int maximum,
                                        int *target, int fallback) {
        if (object.contains(key) && !readInteger(object.value(key), minimum, maximum, target)) {
            *target = fallback;
            invalid = true;
        }
    };
    auto boolField = [&object, &invalid](const QString &key, bool *target, bool fallback) {
        if (object.contains(key) && !readBoolean(object.value(key), target)) {
            *target = fallback;
            invalid = true;
        }
    };
    auto realField = [&object, &invalid](const QString &key, qreal minimum, qreal maximum,
                                         qreal *target, qreal fallback) {
        if (object.contains(key) && !readReal(object.value(key), minimum, maximum, target)) {
            *target = fallback;
            invalid = true;
        }
    };
    auto stringField = [&object, &invalid](const QString &key, QString *target,
                                           const QString &fallback,
                                           const auto &parser, auto *enumTarget,
                                           auto enumFallback) {
        if (!object.contains(key))
            return;
        const QJsonValue value = object.value(key);
        if (!value.isString() || !parser(value.toString(), enumTarget)) {
            *enumTarget = enumFallback;
            *target = fallback;
            invalid = true;
        } else {
            *target = value.toString();
        }
    };

    intField(QStringLiteral("cornerRadius"), 0, 64, &values.cornerRadius, defaults.cornerRadius);
    boolField(QStringLiteral("borderEnabled"), &values.borderEnabled, defaults.borderEnabled);
    intField(QStringLiteral("borderWidth"), 0, 8, &values.borderWidth, defaults.borderWidth);
    if (object.contains(QStringLiteral("borderColor"))
        && !readColor(object.value(QStringLiteral("borderColor")), &values.borderColor)) {
        values.borderColor = defaults.borderColor;
        invalid = true;
    }
    boolField(QStringLiteral("shadowEnabled"), &values.shadowEnabled, defaults.shadowEnabled);
    realField(QStringLiteral("shadowStrength"), 0.0, 1.0, &values.shadowStrength,
              defaults.shadowStrength);

    if (object.contains(QStringLiteral("thumbnailSize"))) {
        const QJsonValue sizeValue = object.value(QStringLiteral("thumbnailSize"));
        if (!sizeValue.isObject()) {
            values.thumbnailWidth = defaults.thumbnailWidth;
            values.thumbnailHeight = defaults.thumbnailHeight;
            invalid = true;
        } else {
            const QJsonObject size = sizeValue.toObject();
            if (size.contains(QStringLiteral("width"))
                && !readInteger(size.value(QStringLiteral("width")), 120, 480,
                                &values.thumbnailWidth)) {
                values.thumbnailWidth = defaults.thumbnailWidth;
                invalid = true;
            }
            if (size.contains(QStringLiteral("height"))
                && !readInteger(size.value(QStringLiteral("height")), 80, 320,
                                &values.thumbnailHeight)) {
                values.thumbnailHeight = defaults.thumbnailHeight;
                invalid = true;
            }
        }
    }
    realField(QStringLiteral("previewMaximumScale"), 0.5, 0.9,
              &values.previewMaximumScale, defaults.previewMaximumScale);
    QString ignored;
    stringField(QStringLiteral("position"), &ignored, positionName(defaults.position),
                parsePosition, &values.position, defaults.position);
    intField(QStringLiteral("edgeMargin"), 0, 120, &values.edgeMargin, defaults.edgeMargin);
    boolField(QStringLiteral("animationsEnabled"), &values.animationsEnabled,
              defaults.animationsEnabled);
    stringField(QStringLiteral("entryAnimation"), &ignored,
                animationName(defaults.entryAnimation), parseAnimation,
                &values.entryAnimation, defaults.entryAnimation);
    stringField(QStringLiteral("exitAnimation"), &ignored,
                animationName(defaults.exitAnimation), parseAnimation,
                &values.exitAnimation, defaults.exitAnimation);
    intField(QStringLiteral("entryDurationMs"), 0, 1500, &values.entryDurationMs,
             defaults.entryDurationMs);
    intField(QStringLiteral("exitDurationMs"), 0, 1500, &values.exitDurationMs,
             defaults.exitDurationMs);
    stringField(QStringLiteral("entryEasing"), &ignored, easingName(defaults.entryEasing),
                parseEasing, &values.entryEasing, defaults.entryEasing);
    stringField(QStringLiteral("exitEasing"), &ignored, easingName(defaults.exitEasing),
                parseEasing, &values.exitEasing, defaults.exitEasing);
    boolField(QStringLiteral("showThumbnail"), &values.showThumbnail, defaults.showThumbnail);
    boolField(QStringLiteral("autoCopy"), &values.autoCopy, defaults.autoCopy);
    boolField(QStringLiteral("dragEnabled"), &values.dragEnabled, defaults.dragEnabled);
    intField(QStringLiteral("displayDurationMs"), 500, 30000,
             &values.displayDurationMs, defaults.displayDurationMs);
    stringField(QStringLiteral("clickAction"), &ignored,
                clickActionName(defaults.clickAction), parseClickAction,
                &values.clickAction, defaults.clickAction);

    *hadInvalidValue = invalid;
    return values;
}

} // namespace

ScreenshotConfiguration::Values ScreenshotConfiguration::defaults()
{
    Values values;
    values.borderColor = QColor(255, 255, 255, 180);
    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        values.borderColor = application->palette().color(QPalette::Light);
        values.borderColor.setAlpha(180);
    }
    return values;
}

ScreenshotConfiguration::ScreenshotConfiguration(QString filePath, QObject *parent)
    : QObject(parent)
    , m_values(defaults())
{
    if (filePath.isEmpty()) {
        QString configRoot = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        if (configRoot.isEmpty())
            configRoot = QDir::homePath() + QStringLiteral("/.config");
        filePath = QDir(configRoot).filePath(QStringLiteral("AstreaOS/screenshot.json"));
    }
    m_filePath = QFileInfo(filePath).absoluteFilePath();
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    m_persistDebounce.setSingleShot(true);
    m_persistDebounce.setInterval(220);
    connect(&m_persistDebounce, &QTimer::timeout, this, [this] { flush(); });
    m_reloadDebounce.setSingleShot(true);
    m_reloadDebounce.setInterval(120);
    connect(&m_reloadDebounce, &QTimer::timeout, this,
            &ScreenshotConfiguration::reloadFromDisk);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this,
            &ScreenshotConfiguration::scheduleReload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            &ScreenshotConfiguration::scheduleReload);

    loadFile(true);
    refreshWatchPaths();
}

ScreenshotConfiguration::~ScreenshotConfiguration()
{
    flush();
}

QByteArray ScreenshotConfiguration::previewPayload() const
{
    return QJsonDocument(encodedValues(m_values)).toJson(QJsonDocument::Compact);
}

bool ScreenshotConfiguration::applyTransientPayload(const QByteArray &payload)
{
    if (payload.size() > 8192)
        return false;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;
    bool invalid = false;
    const Values next = parsedValues(document.object(), defaults(), &invalid);
    if (invalid)
        return false;
    if (!sameValues(next, m_values)) {
        m_values = next;
        emit configurationChanged();
    }
    return true;
}

void ScreenshotConfiguration::discardTransientChanges()
{
    m_persistDebounce.stop();
    m_lastBytes.clear(); // Force a real reload, including updates saved by another process.
    reloadFromDisk();
}

bool ScreenshotConfiguration::flush()
{
    m_persistDebounce.stop();
    if (!m_savePending)
        return true;
    if (!writeCandidate(m_values, {}, true))
        return false;
    m_savePending = false;
    return true;
}

QString ScreenshotConfiguration::position() const
{
    return positionName(m_values.position);
}

QString ScreenshotConfiguration::entryAnimation() const
{
    return animationName(m_values.entryAnimation);
}

QString ScreenshotConfiguration::exitAnimation() const
{
    return animationName(m_values.exitAnimation);
}

QString ScreenshotConfiguration::entryEasing() const
{
    return easingName(m_values.entryEasing);
}

QString ScreenshotConfiguration::exitEasing() const
{
    return easingName(m_values.exitEasing);
}

QString ScreenshotConfiguration::clickAction() const
{
    return clickActionName(m_values.clickAction);
}

void ScreenshotConfiguration::update(const QString &changedField, const Values &candidate)
{
    if (sameValues(candidate, m_values))
        return;
    if (!m_writable) {
        setStatus(QStringLiteral("error"),
                  QStringLiteral("Reset screenshot settings to replace the unreadable configuration"));
        return;
    }
    m_values = candidate;
    m_savePending = true;
    m_persistDebounce.start();
    setStatus(QStringLiteral("editing"));
    emit configurationChanged();
}

void ScreenshotConfiguration::setCornerRadius(int value)
{
    Values next = m_values;
    next.cornerRadius = std::clamp(value, 0, 64);
    update(QStringLiteral("cornerRadius"), next);
}

void ScreenshotConfiguration::setBorderEnabled(bool value)
{
    Values next = m_values;
    next.borderEnabled = value;
    update(QStringLiteral("borderEnabled"), next);
}

void ScreenshotConfiguration::setBorderWidth(int value)
{
    Values next = m_values;
    next.borderWidth = std::clamp(value, 0, 8);
    update(QStringLiteral("borderWidth"), next);
}

void ScreenshotConfiguration::setBorderColor(const QColor &value)
{
    if (!value.isValid())
        return;
    Values next = m_values;
    next.borderColor = value;
    update(QStringLiteral("borderColor"), next);
}

void ScreenshotConfiguration::setShadowEnabled(bool value)
{
    Values next = m_values;
    next.shadowEnabled = value;
    update(QStringLiteral("shadowEnabled"), next);
}

void ScreenshotConfiguration::setShadowStrength(qreal value)
{
    if (!std::isfinite(value))
        return;
    Values next = m_values;
    next.shadowStrength = std::clamp(value, 0.0, 1.0);
    update(QStringLiteral("shadowStrength"), next);
}

void ScreenshotConfiguration::setThumbnailWidth(int value)
{
    Values next = m_values;
    next.thumbnailWidth = std::clamp(value, 120, 480);
    update(QStringLiteral("thumbnailWidth"), next);
}

void ScreenshotConfiguration::setThumbnailHeight(int value)
{
    Values next = m_values;
    next.thumbnailHeight = std::clamp(value, 80, 320);
    update(QStringLiteral("thumbnailHeight"), next);
}

void ScreenshotConfiguration::setPreviewMaximumScale(qreal value)
{
    if (!std::isfinite(value))
        return;
    Values next = m_values;
    next.previewMaximumScale = std::clamp(value, 0.5, 0.9);
    update(QStringLiteral("previewMaximumScale"), next);
}

void ScreenshotConfiguration::setPosition(const QString &value)
{
    Values next = m_values;
    if (!parsePosition(value, &next.position))
        return;
    update(QStringLiteral("position"), next);
}

void ScreenshotConfiguration::setEdgeMargin(int value)
{
    Values next = m_values;
    next.edgeMargin = std::clamp(value, 0, 120);
    update(QStringLiteral("edgeMargin"), next);
}

void ScreenshotConfiguration::setAnimationsEnabled(bool value)
{
    Values next = m_values;
    next.animationsEnabled = value;
    update(QStringLiteral("animationsEnabled"), next);
}

void ScreenshotConfiguration::setEntryAnimation(const QString &value)
{
    Values next = m_values;
    if (!parseAnimation(value, &next.entryAnimation))
        return;
    update(QStringLiteral("entryAnimation"), next);
}

void ScreenshotConfiguration::setExitAnimation(const QString &value)
{
    Values next = m_values;
    if (!parseAnimation(value, &next.exitAnimation))
        return;
    update(QStringLiteral("exitAnimation"), next);
}

void ScreenshotConfiguration::setEntryDurationMs(int value)
{
    Values next = m_values;
    next.entryDurationMs = std::clamp(value, 0, 1500);
    update(QStringLiteral("entryDurationMs"), next);
}

void ScreenshotConfiguration::setExitDurationMs(int value)
{
    Values next = m_values;
    next.exitDurationMs = std::clamp(value, 0, 1500);
    update(QStringLiteral("exitDurationMs"), next);
}

void ScreenshotConfiguration::setEntryEasing(const QString &value)
{
    Values next = m_values;
    if (!parseEasing(value, &next.entryEasing))
        return;
    update(QStringLiteral("entryEasing"), next);
}

void ScreenshotConfiguration::setExitEasing(const QString &value)
{
    Values next = m_values;
    if (!parseEasing(value, &next.exitEasing))
        return;
    update(QStringLiteral("exitEasing"), next);
}

void ScreenshotConfiguration::setShowThumbnail(bool value)
{
    Values next = m_values;
    next.showThumbnail = value;
    update(QStringLiteral("showThumbnail"), next);
}

void ScreenshotConfiguration::setAutoCopy(bool value)
{
    Values next = m_values;
    next.autoCopy = value;
    update(QStringLiteral("autoCopy"), next);
}

void ScreenshotConfiguration::setDragEnabled(bool value)
{
    Values next = m_values;
    next.dragEnabled = value;
    update(QStringLiteral("dragEnabled"), next);
}

void ScreenshotConfiguration::setDisplayDurationMs(int value)
{
    Values next = m_values;
    next.displayDurationMs = std::clamp(value, 500, 30000);
    update(QStringLiteral("displayDurationMs"), next);
}

void ScreenshotConfiguration::setClickAction(const QString &value)
{
    Values next = m_values;
    if (!parseClickAction(value, &next.clickAction))
        return;
    update(QStringLiteral("clickAction"), next);
}

void ScreenshotConfiguration::resetToDefaults()
{
    Values next = defaults();
    m_persistDebounce.stop();
    m_savePending = false;
    const bool writeAllFields = true;
    if (writeCandidate(next, {}, writeAllFields)) {
        m_values = next;
        emit configurationChanged();
    }
}

bool ScreenshotConfiguration::loadFile(bool initialLoad)
{
    Values fallback = defaults();
    QFile file(m_filePath);
    if (!file.exists()) {
        const bool changed = !sameValues(m_values, fallback);
        m_values = fallback;
        m_rootObject = {};
        m_lastBytes.clear();
        m_writable = true;
        m_hasInvalidFields = false;
        setStatus(QStringLiteral("ready"));
        if (!initialLoad && changed)
            emit configurationChanged();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly) || file.size() > kMaximumFileBytes) {
        m_values = fallback;
        m_rootObject = {};
        m_lastBytes.clear();
        m_writable = false;
        m_hasInvalidFields = false;
        setStatus(QStringLiteral("error"), QStringLiteral("Configuration file is unreadable or exceeds 64 KiB"));
        if (!initialLoad)
            emit configurationChanged();
        return false;
    }

    const QByteArray bytes = file.read(kMaximumFileBytes + 1);
    if (bytes.size() > kMaximumFileBytes) {
        m_values = fallback;
        m_rootObject = {};
        m_lastBytes = bytes;
        m_writable = false;
        m_hasInvalidFields = false;
        setStatus(QStringLiteral("error"), QStringLiteral("Configuration file exceeds 64 KiB"));
        if (!initialLoad)
            emit configurationChanged();
        return false;
    }
    if (!initialLoad && bytes == m_lastBytes)
        return true;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_values = fallback;
        m_rootObject = {};
        m_lastBytes = bytes;
        m_writable = false;
        m_hasInvalidFields = false;
        setStatus(QStringLiteral("error"), QStringLiteral("Configuration file must contain a JSON object"));
        if (!initialLoad)
            emit configurationChanged();
        return false;
    }

    const Values previous = m_values;
    m_rootObject = document.object();
    bool invalidValue = false;
    m_values = parsedValues(m_rootObject, fallback, &invalidValue);
    m_lastBytes = bytes;
    m_writable = true;
    m_hasInvalidFields = invalidValue;
    setStatus(invalidValue ? QStringLiteral("error") : QStringLiteral("ready"),
              invalidValue ? QStringLiteral("One or more screenshot settings are invalid") : QString());
    if (!initialLoad && !sameValues(m_values, previous))
        emit configurationChanged();
    return true;
}

bool ScreenshotConfiguration::writeCandidate(const Values &candidate,
                                               const QString &changedField,
                                               bool writeEveryField)
{
    if (!m_writable && !writeEveryField) {
        setStatus(QStringLiteral("error"), QStringLiteral("Reset screenshot settings to replace the unreadable configuration"));
        return false;
    }
    writeEveryField = writeEveryField || m_hasInvalidFields;
    QJsonObject next = m_rootObject;
    const QJsonObject encoded = encodedValues(candidate);
    if (writeEveryField) {
        for (auto it = encoded.cbegin(); it != encoded.cend(); ++it) {
            if (it.key() == QStringLiteral("thumbnailSize")) {
                QJsonObject size = next.value(QStringLiteral("thumbnailSize")).toObject();
                size.insert(QStringLiteral("width"), candidate.thumbnailWidth);
                size.insert(QStringLiteral("height"), candidate.thumbnailHeight);
                next.insert(it.key(), size);
            } else {
                next.insert(it.key(), it.value());
            }
        }
    } else if (changedField == QStringLiteral("thumbnailWidth")
               || changedField == QStringLiteral("thumbnailHeight")) {
        QJsonObject size = next.value(QStringLiteral("thumbnailSize")).toObject();
        size.insert(QStringLiteral("width"), candidate.thumbnailWidth);
        size.insert(QStringLiteral("height"), candidate.thumbnailHeight);
        next.insert(QStringLiteral("thumbnailSize"), size);
    } else {
        next.insert(changedField, encoded.value(changedField));
    }

    const QByteArray bytes = QJsonDocument(next).toJson(QJsonDocument::Indented);
    if (bytes.size() > kMaximumFileBytes) {
        setStatus(QStringLiteral("error"), QStringLiteral("Configuration file would exceed 64 KiB"));
        return false;
    }
    if (!QDir().mkpath(QFileInfo(m_filePath).absolutePath())) {
        setStatus(QStringLiteral("error"), QStringLiteral("Configuration directory is unavailable"));
        return false;
    }
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        setStatus(QStringLiteral("error"), file.errorString());
        return false;
    }
    m_rootObject = next;
    m_lastBytes = bytes;
    m_writable = true;
    m_hasInvalidFields = false;
    setStatus(QStringLiteral("saved"));
    refreshWatchPaths();
    return true;
}

void ScreenshotConfiguration::setStatus(QString status, QString error)
{
    if (m_status == status && m_lastError == error)
        return;
    m_status = std::move(status);
    m_lastError = std::move(error);
    emit statusChanged();
}

void ScreenshotConfiguration::scheduleReload()
{
    m_reloadDebounce.start();
}

void ScreenshotConfiguration::refreshWatchPaths()
{
    const QString parent = QFileInfo(m_filePath).absolutePath();
    if (!QDir().mkpath(parent))
        return;
    if (!m_watcher.directories().contains(parent))
        m_watcher.addPath(parent);
    if (QFileInfo::exists(m_filePath) && !m_watcher.files().contains(m_filePath))
        m_watcher.addPath(m_filePath);
}

void ScreenshotConfiguration::reloadFromDisk()
{
    if (m_savePending)
        return;
    loadFile(false);
    refreshWatchPaths();
}
