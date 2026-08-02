#pragma once

#include <QStringList>

struct CommandLineRequest {
    enum class Mode { Daemon, Status, Reload, Show, Hide, Quit };

    Mode mode = Mode::Daemon;
    bool daemonMode = false;
};

class CommandLine final {
public:
    static CommandLineRequest parse(const QStringList &args);
};
