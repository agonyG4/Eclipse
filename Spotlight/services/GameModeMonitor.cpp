#include "services/GameModeMonitor.hpp"
#include <QDebug>

GameModeMonitor::GameModeMonitor(QObject *parent)
    : QObject(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &GameModeMonitor::poll);
}

void GameModeMonitor::start(int intervalMs) {
    m_timer->start(intervalMs);
    poll();
}

void GameModeMonitor::stop() {
    m_timer->stop();
}

void GameModeMonitor::poll() {
    // Prevent overlapping polls
    if (m_proc && m_proc->state() != QProcess::NotRunning)
        return;

    m_proc = new QProcess(this);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        QByteArray output;
        if (exitCode == 0) {
            output = m_proc->readAllStandardOutput();
            QByteArray errOutput = m_proc->readAllStandardError();
            if (!errOutput.isEmpty())
                output += "\n" + errOutput;
        }
        bool wasActive = m_active;
        m_active = parseGameModeOutput(output);
        m_proc->deleteLater();
        m_proc = nullptr;
        if (wasActive != m_active)
            emit gameModeChanged();
    });

    // Timeout: kill stuck process
    QTimer::singleShot(3000, m_proc, [this] {
        if (m_proc && m_proc->state() != QProcess::NotRunning) {
            m_proc->kill();
        }
    });

    m_proc->setProgram(QStringLiteral("gamemoded"));
    m_proc->setArguments({QStringLiteral("-s")});
    m_proc->start();
}

bool GameModeMonitor::parseGameModeOutput(const QByteArray &output) {
    QString text = QString::fromUtf8(output).trimmed().toLower();
    if (text.contains(QStringLiteral("is not active")) ||
        text.contains(QStringLiteral("inactive")))
        return false;
    if (text.contains(QStringLiteral("is active")))
        return true;
    // Also accept exact "active" for compatibility
    if (text == QStringLiteral("active"))
        return true;
    return false;
}
