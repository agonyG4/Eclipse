#pragma once

#include <QString>
#include <QStringList>

struct CommandLineRequest {
    enum class Mode {
        Daemon,
        Next,
        Previous,
        Commit,
        Cancel,
        Show,
        Hide,
        ReloadWindows,
        Status,
        Unknown
    };

    Mode mode = Mode::Daemon;
    QString argument;
    QString backend = QStringLiteral("auto");
    bool daemonMode = false;
};

class CommandLine final {
public:
    static CommandLineRequest parse(const QStringList &args);
};
