#pragma once

#include "statusnotifier/StatusNotifierTypes.hpp"

#include <QObject>

namespace Astrea::StatusNotifier {

class StatusNotifierItemProxy final : public QObject {
    Q_OBJECT

public:
    StatusNotifierItemProxy(const ItemAddress &address, quint64 generation,
                            QObject *parent = nullptr);

    const ItemAddress &address() const { return m_address; }
    quint64 generation() const { return m_generation; }
    const ItemSnapshot &snapshot() const { return m_snapshot; }

    void start();
    void stop();
    void activate(int x, int y);
    void secondaryActivate(int x, int y);
    void contextMenu(int x, int y);
    void scroll(int delta, const QString &orientation);

signals:
    void snapshotChanged(const ItemSnapshot &snapshot);
    void vanished(const QString &key, const QString &uniqueOwner);
    void actionFailed(const QString &key, const QString &error);

private slots:
    void onPropertiesChanged(const QString &interfaceName, const QVariantMap &changed,
                             const QStringList &invalidated);
    void onNewTitle();
    void onNewIcon();
    void onNewAttentionIcon();
    void onNewOverlayIcon();
    void onNewToolTip();
    void onNewStatus(const QString &status);

private:
    void refresh(const QString &interfaceName, bool allowFallback);
    void applyProperties(const QString &interfaceName, const QVariantMap &properties);
    void connectSignals(const QString &interfaceName);
    void disconnectSignals();
    void callAction(const QString &method, const QVariantList &arguments);
    void emitSnapshot();

    ItemAddress m_address;
    quint64 m_generation = 0;
    quint64 m_requestGeneration = 0;
    ItemSnapshot m_snapshot;
    QString m_interfaceName;
    QString m_connectedInterface;
    bool m_started = false;
};

} // namespace Astrea::StatusNotifier
