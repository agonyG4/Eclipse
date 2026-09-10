#include "services/animation/SettingsAnimationController.hpp"

#include "services/animation/SettingsTyphonControlClient.hpp"

#include <QVariant>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

SettingsAnimationController::SettingsAnimationController(QObject *parent)
    : QObject(parent)
    , m_client(std::make_unique<SettingsTyphonControlClient>(this))
{
    m_speedFlushTimer.setSingleShot(true);
    m_speedFlushTimer.setInterval(80);
    connect(&m_speedFlushTimer, &QTimer::timeout, this, &SettingsAnimationController::flush);
    connect(m_client.get(), &SettingsTyphonControlClient::requestFinished, this,
            &SettingsAnimationController::handleRequestFinished);
}

SettingsAnimationController::~SettingsAnimationController() = default;

void SettingsAnimationController::refresh()
{
    if (m_busy)
        return;
    if (m_available && (m_pendingSpeed >= 0.0 || !m_pendingMutations.empty())) {
        submitPendingMutations();
        return;
    }

    m_activeRequestIsRefresh = true;
    m_busy = true;
    emit busyChanged();
    QString error;
    if (m_client->startRequest(QStringLiteral("animation.config.get"), {}, &error))
        return;

    m_activeRequestIsRefresh = false;
    m_busy = false;
    emit busyChanged();
    if (m_available) {
        m_available = false;
        emit availabilityChanged();
    }
    setError(error);
}

void SettingsAnimationController::handleRequestFinished(bool success,
                                                         const QVariantMap &result,
                                                         const QString &error)
{
    const bool refresh = m_activeRequestIsRefresh;
    m_activeRequestIsRefresh = false;
    if (m_busy) {
        m_busy = false;
        emit busyChanged();
    }

    if (success) {
        if (!m_available) {
            m_available = true;
            emit availabilityChanged();
        }
        if (applySnapshot(result))
            setError({});
    } else {
        if (refresh || !m_client->lastFailureWasServerRejection()) {
            if (m_available) {
                m_available = false;
                emit availabilityChanged();
            }
        }
        setError(error);
    }
    submitPendingMutations();
}

void SettingsAnimationController::setEnabled(bool value)
{
    enqueueMutation([value](QVariantMap &configuration) {
        configuration.insert(QStringLiteral("enabled"), value);
    });
}

void SettingsAnimationController::setPreset(const QString &value)
{
    enqueueMutation([value](QVariantMap &configuration) {
        configuration.insert(QStringLiteral("preset"), value);
    });
}

void SettingsAnimationController::setSpeed(double value)
{
    if (!m_available || !std::isfinite(value))
        return;
    m_pendingSpeed = qBound(0.5, value, 2.0);
    m_speedFlushTimer.start();
}

void SettingsAnimationController::setSlotEffect(const QString &slotId, const QString &effectId)
{
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

    enqueueMutation([slotId, effectId](QVariantMap &configuration) {
        QVariantMap overrides = configuration.value(QStringLiteral("overrides")).toMap();
        overrides.insert(slotId, effectId);
        configuration.insert(QStringLiteral("overrides"), overrides);
    });
}

void SettingsAnimationController::clearSlotOverride(const QString &slotId)
{
    enqueueMutation([slotId](QVariantMap &configuration) {
        QVariantMap overrides = configuration.value(QStringLiteral("overrides")).toMap();
        overrides.remove(slotId);
        configuration.insert(QStringLiteral("overrides"), overrides);
    });
}

void SettingsAnimationController::resetOverrides()
{
    enqueueMutation([](QVariantMap &configuration) {
        configuration.insert(QStringLiteral("overrides"), QVariantMap{});
    });
}

void SettingsAnimationController::restoreDefaults()
{
    const QVariantMap defaults = defaultConfiguration();
    enqueueMutation([defaults](QVariantMap &configuration) {
        configuration = defaults;
    });
}

void SettingsAnimationController::flush()
{
    m_speedFlushTimer.stop();
    submitPendingMutations();
}

void SettingsAnimationController::enqueueMutation(ConfigurationMutation mutation)
{
    if (!m_available)
        return;
    m_pendingMutations.push_back(std::move(mutation));
    submitPendingMutations();
}

void SettingsAnimationController::submitPendingMutations()
{
    if (m_busy || !m_available
        || (m_pendingSpeed < 0.0 && m_pendingMutations.empty())) {
        return;
    }

    QVariantMap configuration = m_config;
    for (const ConfigurationMutation &mutation : m_pendingMutations)
        mutation(configuration);
    if (m_pendingSpeed >= 0.0)
        configuration.insert(QStringLiteral("speed"), m_pendingSpeed);

    if (submit(configuration)) {
        m_pendingMutations.clear();
        m_pendingSpeed = -1.0;
        m_speedFlushTimer.stop();
    } else if (!m_busy) {
        m_pendingMutations.clear();
        m_pendingSpeed = -1.0;
        m_speedFlushTimer.stop();
    }
}

bool SettingsAnimationController::applySnapshot(const QVariantMap &snapshot)
{
    if (!snapshot.contains(QStringLiteral("config"))) {
        setError(QStringLiteral("Typhon returned an incomplete animation snapshot."));
        return false;
    }
    m_snapshot = snapshot;
    m_config = snapshot.value(QStringLiteral("config")).toMap();
    rebuildCapabilities();
    emit snapshotChanged();
    return true;
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

    QVariantMap arguments = configuration;
    arguments.insert(QStringLiteral("version"), 1);
    m_activeRequestIsRefresh = false;
    m_busy = true;
    emit busyChanged();
    QString error;
    if (m_client->startRequest(QStringLiteral("animation.config.set"), arguments, &error))
        return true;

    m_busy = false;
    emit busyChanged();
    if (m_available) {
        m_available = false;
        emit availabilityChanged();
    }
    setError(error);
    return false;
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
