#pragma once

#include <QByteArray>
#include <QString>

class ShellIpcClient final {
public:
    static bool send(const QString &serverName, const QString &line,
                     int timeoutMs = 500);
    static QByteArray requestReply(const QString &serverName, const QString &line,
                                   int timeoutMs = 500);
};
