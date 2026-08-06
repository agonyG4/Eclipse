#pragma once

#include <QObject>
#include <QString>

class TyphonWaylandDisplay : public QObject {
    Q_OBJECT

public:
    explicit TyphonWaylandDisplay(QObject *parent = nullptr) : QObject(parent) {}
    ~TyphonWaylandDisplay() override = default;

    virtual bool connectToDisplay(const QString &displayName = {}) = 0;
    virtual void disconnectFromDisplay() = 0;
    virtual int fileDescriptor() const = 0;
    virtual bool dispatchPending() = 0;
    virtual bool flush() = 0;

    void readable();
    void disconnected();
    void protocolError(QString diagnostic);
};
