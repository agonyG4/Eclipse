#pragma once

#include <QLocalSocket>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

class SettingsTyphonControlClient final : public QObject {
    Q_OBJECT

public:
    explicit SettingsTyphonControlClient(QObject *parent = nullptr, int deadlineMs = 2000);

    bool startRequest(const QString &command, const QVariantMap &arguments,
                      QString *startError = nullptr);
    bool busy() const { return m_busy; }
    bool lastFailureWasServerRejection() const { return m_lastFailureWasServerRejection; }

signals:
    void requestFinished(bool success, const QVariantMap &result, const QString &error);

private slots:
    void handleConnected();
    void handleReadyRead();
    void handleSocketError(QLocalSocket::LocalSocketError error);
    void handleDisconnected();
    void handleDeadline();

private:
    void complete(bool success, const QVariantMap &result, const QString &error,
                  bool serverRejection = false);
    bool parseResponse(const QByteArray &line, QVariantMap *result, QString *error);
    QString discoverSocket(QString *error) const;
    static bool validInstanceName(const QString &value);
    static bool secureDirectory(const QString &path);
    static bool secureSocket(const QString &path);

    QLocalSocket m_socket;
    QTimer m_deadlineTimer;
    QByteArray m_encodedRequest;
    QByteArray m_response;
    quint64 m_nextRequestId = 1;
    quint64 m_activeRequestId = 0;
    int m_deadlineMs = 2000;
    bool m_busy = false;
    bool m_lastFailureWasServerRejection = false;
};
