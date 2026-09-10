#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <vector>

class SettingsTyphonControlClient;

class SettingsAnimationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY snapshotChanged)
    Q_PROPERTY(QString preset READ preset NOTIFY snapshotChanged)
    Q_PROPERTY(double speed READ speed NOTIFY snapshotChanged)
    Q_PROPERTY(quint64 generation READ generation NOTIFY snapshotChanged)
    Q_PROPERTY(QString source READ source NOTIFY snapshotChanged)
    Q_PROPERTY(bool hasOverrides READ hasOverrides NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList slots READ slotCapabilities NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList presets READ presets NOTIFY snapshotChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)

public:
    explicit SettingsAnimationController(QObject *parent = nullptr);
    ~SettingsAnimationController() override;

    bool available() const { return m_available; }
    bool busy() const { return m_busy; }
    bool enabled() const { return m_config.value(QStringLiteral("enabled")).toBool(); }
    QString preset() const { return m_config.value(QStringLiteral("preset")).toString(); }
    double speed() const { return m_config.value(QStringLiteral("speed")).toDouble(); }
    quint64 generation() const { return m_snapshot.value(QStringLiteral("generation")).toULongLong(); }
    QString source() const { return m_snapshot.value(QStringLiteral("source")).toString(); }
    bool hasOverrides() const { return !m_config.value(QStringLiteral("overrides")).toMap().isEmpty(); }
    QVariantList slotCapabilities() const { return m_slots; }
    QVariantList presets() const { return m_presets; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setEnabled(bool value);
    Q_INVOKABLE void setPreset(const QString &value);
    Q_INVOKABLE void setSpeed(double value);
    Q_INVOKABLE void setSlotEffect(const QString &slotId, const QString &effectId);
    Q_INVOKABLE void clearSlotOverride(const QString &slotId);
    Q_INVOKABLE void resetOverrides();
    Q_INVOKABLE void restoreDefaults();
    Q_INVOKABLE void flush();

signals:
    void availabilityChanged();
    void busyChanged();
    void snapshotChanged();
    void errorChanged();

private:
    using ConfigurationMutation = std::function<void(QVariantMap &)>;

    bool applySnapshot(const QVariantMap &snapshot);
    void setError(const QString &message);
    bool submit(const QVariantMap &configuration);
    void enqueueMutation(ConfigurationMutation mutation);
    void submitPendingMutations();
    void handleRequestFinished(bool success, const QVariantMap &result, const QString &error);
    QVariantMap defaultConfiguration() const;
    void rebuildCapabilities();

    std::unique_ptr<SettingsTyphonControlClient> m_client;
    QVariantMap m_snapshot;
    QVariantMap m_config;
    QVariantList m_slots;
    QVariantList m_presets;
    QTimer m_speedFlushTimer;
    std::vector<ConfigurationMutation> m_pendingMutations;
    double m_pendingSpeed = -1.0;
    QString m_lastError;
    bool m_available = false;
    bool m_busy = false;
    bool m_activeRequestIsRefresh = false;
};
