#pragma once

#include "platform/typhon/TyphonProtocolTypes.hpp"

#include <QObject>

class TyphonProtocolAdapter : public QObject {
    Q_OBJECT

public:
    explicit TyphonProtocolAdapter(QObject *parent = nullptr) : QObject(parent) {}
    ~TyphonProtocolAdapter() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isAvailable() const = 0;

signals:
    void registryDiscovered(bool managerAvailable);
    void handleCreated(quint64 token);
    void identifierChanged(quint64 token, QString id);
    void appIdChanged(quint64 token, QString appId);
    void titleChanged(quint64 token, QString title);
    void pidChanged(quint64 token, quint32 pid);
    void kindChanged(quint64 token, Astrea::Typhon::ToplevelKind kind);
    void stateChanged(quint64 token, Astrea::Typhon::ToplevelStates states, quint32 rawStateBits);
    void focusSerialChanged(quint64 token, Astrea::Typhon::FocusSerial serial);
    void handleCompleted(quint64 token, Astrea::Typhon::Revision revision);
    void handleClosed(quint64 token);
    void managerCompleted(Astrea::Typhon::Revision revision, quint32 total, bool truncated);
    void managerFailed(QString diagnostic);
    void displayDisconnected();
    void protocolError(QString diagnostic);
};

TyphonProtocolAdapter *createDefaultTyphonProtocolAdapter(QObject *parent = nullptr);
