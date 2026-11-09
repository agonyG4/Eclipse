#pragma once

#include <QHash>
#include <QObject>
#include <QVariantMap>

#include <QDBusConnection>
#include <QDBusMessage>

class DockAppModel;

class UnityLauncherEntryService final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.Unity.LauncherEntry")

public:
    explicit UnityLauncherEntryService(DockAppModel *model,
                                      QDBusConnection connection = QDBusConnection::sessionBus(),
                                      QObject *parent = nullptr);
    ~UnityLauncherEntryService() override;

    bool start();
    void stop();
    bool isStarted() const { return m_started; }

signals:
    // One sender's own entry, for consumers that tie a counter to its process.
    void senderUnreadChanged(const QString &sender, const QString &desktopFileName, int count,
                             bool visible);

private slots:
    void onUpdate(const QString &applicationUri, const QVariantMap &properties,
                  const QDBusMessage &message);
    void onNameOwnerChanged(const QString &name, const QString &oldOwner,
                            const QString &newOwner);

private:
    struct SenderState {
        bool hasCount = false;
        int count = 0;
        quint64 countOrder = 0;
        bool hasVisibility = false;
        bool visible = false;
        quint64 visibilityOrder = 0;
    };

    static QString desktopFileNameFromUri(const QString &applicationUri);
    static bool integerCount(const QVariant &value, int *count);
    void clearSender(const QString &sender);
    void refreshDesktop(const QString &desktopFileName);

    DockAppModel *m_model = nullptr;
    QDBusConnection m_connection;
    QHash<QString, QHash<QString, SenderState>> m_senderStates;
    quint64 m_updateOrder = 0;
    bool m_ownsService = false;
    bool m_registeredUnityObject = false;
    bool m_started = false;
};
