#pragma once

#include <QString>
#include <QStringList>

struct CommandLineRequest {
    enum class Mode {
        Daemon,
        SendCommand,
        Status,
        ResolveIcon,
        IconThemeDiagnostics
    };

    Mode mode = Mode::SendCommand;
    QString command = QStringLiteral("show");
    QString argument;
    bool daemonMode = false;
};

class CommandLine final {
public:
    static CommandLineRequest parse(const QStringList &args);
};
