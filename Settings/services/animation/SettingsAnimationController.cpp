#include "services/animation/SettingsAnimationController.hpp"

#include "services/animation/SettingsTyphonControlClient.hpp"

#include <QVariant>

#include <algorithm>
#include <cmath>
#include <memory>

SettingsAnimationController::SettingsAnimationController(QObject *parent)
    : QObject(parent)
    , m_client(std::make_unique<SettingsTyphonControlClient>(this))
{
    m_speedFlushTimer.setSingleShot(true);
    m_speedFlushTimer.setInterval(80);
    connect(&m_speedFlushTimer, &QTimer::timeout, this, &SettingsAnimationController::flush);
    refresh();
}

SettingsAnimationController::~SettingsAnimationController() = default;

void SettingsAnimationController::refresh()
{
    if (m_busy)
        return;
    m_busy = true;
    emit busyChanged();
    QVariantMap snapshot;
    QString error;
    const bool ok = m_client->request(QStringLiteral("animation.config.get"), {}, &snapshot, &error);
    m_busy = false;
    emit busyChanged();
    if (!ok) {
        m_available = false;
        emit availabilityChanged();
        setError(error);
        return;
    }
    m_available = true;
    emit availabilityChanged();
    applySnapshot(snapshot);
    setError({});
}

void SettingsAnimationController::setEnabled(bool value)
{
    flush();
    QVariantMap configuration = m_config;
    configuration.insert(QStringLiteral("enabled"), value);
    submit(configuration);
}

void SettingsAnimationController::setPreset(const QString &value)
{
    flush();
    QVariantMap configuration = m_config;
    configuration.insert(QStringLiteral("preset"), value);
    submit(configuration);
}

void SettingsAnimationController::setSpeed(double value)
{
    if (!std::isfinite(value))
        return;
    m_pendingSpeed = qBound(0.5, value, 2.0);
    m_speedFlushTimer.start();
}

void SettingsAnimationController::setSlotEffect(const QString &slotId, const QString &effectId)
{
    flush();
    const QVariantMap configuration = m_config;
    const QVariantList capabilities = m_snapshot.value(QStringLiteral("catalog")).toMap()
                                          .value(QStringLiteral("slots"))
                                          .toList();
    bool compatible = false;
    for (const QVariant &value : capabilities) {
        const QVariantMap slot = value.toMap();
        const QVariantList compatibleEffects = slot.value(QStringLiteral("compatibleEffects")).toList();
        if (slot.value(QStringLiteral("id")).toString() == slotId
            && std::any_of(compatibleEffects.cbegin(), compatibleEffects.cend(), [&](const QVariant &value) {
                   return value.toString() == effectId;
               })) {
            compatible = true;
            break;
        }
    }
    const QVariantList effects = m_snapshot.value(QStringLiteral("catalog")).toMap()
                                     .value(QStringLiteral("effects"))
                                     .toList();
    bool available = false;
    for (const QVariant &value : effects) {
        const QVariantMap effect = value.toMap();
        if (effect.value(QStringLiteral("id")).toString() == effectId
            && effect.value(QStringLiteral("availability")).toString() == QStringLiteral("available")) {
            available = true;
            break;
        }
    }
    if (!compatible || !available) {
        setError(QStringLiteral("The selected animation is unavailable for this slot."));
        return;
    }
    QVariantMap updated = configuration;
    QVariantMap overrides = updated.value(QStringLiteral("overrides")).toMap();
    overrides.insert(slotId, effectId);
    updated.insert(QStringLiteral("overrides"), overrides);
    submit(updated);
}

void SettingsAnimationController::clearSlotOverride(const QString &slotId)
{
    flush();
    QVariantMap configuration = m_config;
    QVariantMap overrides = configuration.value(QStringLiteral("overrides")).toMap();
    overrides.remove(slotId);
    configuration.insert(QStringLiteral("overrides"), overrides);
    submit(configuration);
}

void SettingsAnimationController::resetOverrides()
{
    flush();
    QVariantMap configuration = m_config;
    configuration.insert(QStringLiteral("overrides"), QVariantMap{});
    submit(configuration);
}

void SettingsAnimationController::restoreDefaults()
{
    flush();
    submit(defaultConfiguration());
}

void SettingsAnimationController::flush()
{
    if (m_pendingSpeed < 0.0)
        return;
    m_speedFlushTimer.stop();
    QVariantMap configuration = m_config;
    configuration.insert(QStringLiteral("speed"), m_pendingSpeed);
    m_pendingSpeed = -1.0;
    submit(configuration);
}

void SettingsAnimationController::applySnapshot(const QVariantMap &snapshot)
{
    if (!snapshot.contains(QStringLiteral("config"))) {
        setError(QStringLiteral("Typhon returned an incomplete animation snapshot."));
        return;
    }
    m_snapshot = snapshot;
    m_config = snapshot.value(QStringLiteral("config")).toMap();
    rebuildCapabilities();
    emit snapshotChanged();
}

void SettingsAnimationController::setError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    emit errorChanged();
}

bool SettingsAnimationController::submit(const QVariantMap &configuration)
{
    if (m_busy || !m_available)
        return false;
    m_busy = true;
    emit busyChanged();
    QVariantMap arguments = configuration;
    arguments.insert(QStringLiteral("version"), 1);
    QVariantMap snapshot;
    QString error;
    const bool ok = m_client->request(QStringLiteral("animation.config.set"), arguments, &snapshot, &error);
    m_busy = false;
    emit busyChanged();
    if (!ok) {
        setError(error);
        return false;
    }
    applySnapshot(snapshot);
    setError({});
    return true;
}

QVariantMap SettingsAnimationController::defaultConfiguration() const
{
    return {{QStringLiteral("enabled"), true}, {QStringLiteral("preset"), QStringLiteral("astrea")},
            {QStringLiteral("speed"), 1.0}, {QStringLiteral("overrides"), QVariantMap{}}};
}

void SettingsAnimationController::rebuildCapabilities()
{
    m_slots.clear();
    m_presets.clear();
    const QVariantMap catalog = m_snapshot.value(QStringLiteral("catalog")).toMap();
    for (const QVariant &preset : catalog.value(QStringLiteral("presets")).toList())
        m_presets.append(preset.toMap().value(QStringLiteral("id")));
    const QVariantMap requested = m_snapshot.value(QStringLiteral("requested")).toMap();
    const QVariantMap effective = m_snapshot.value(QStringLiteral("effective")).toMap();
    const QVariantMap overrides = m_config.value(QStringLiteral("overrides")).toMap();
    for (const QVariant &value : catalog.value(QStringLiteral("slots")).toList()) {
        QVariantMap slot = value.toMap();
        const QString id = slot.value(QStringLiteral("id")).toString();
        QStringList available;
        QStringList planned;
        for (const QVariant &effectValue : slot.value(QStringLiteral("compatibleEffects")).toList()) {
            const QString effectId = effectValue.toString();
            QString availability;
            for (const QVariant &effectEntry : catalog.value(QStringLiteral("effects")).toList()) {
                const QVariantMap effect = effectEntry.toMap();
                if (effect.value(QStringLiteral("id")).toString() == effectId) {
                    availability = effect.value(QStringLiteral("availability")).toString();
                    break;
                }
            }
            (availability == QStringLiteral("available") ? available : planned).append(effectId);
        }
        slot.insert(QStringLiteral("availableEffects"), available);
        slot.insert(QStringLiteral("plannedEffects"), planned);
        slot.insert(QStringLiteral("requested"), requested.value(id));
        slot.insert(QStringLiteral("effective"), effective.value(id));
        slot.insert(QStringLiteral("override"), overrides.value(id));
        m_slots.append(slot);
    }
}
