#pragma once

#include <QObject>
#include <QTimer>
#include <QProcess>

class GameModeMonitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool gameModeActive READ gameModeActive NOTIFY gameModeChanged)

public:
    explicit GameModeMonitor(QObject *parent = nullptr);

    bool gameModeActive() const { return m_active; }
    static bool parseGameModeOutput(const QByteArray &output);

signals:
    void gameModeChanged();

public slots:
    void poll();
    void start(int intervalMs = 10000);
    void stop();

private:
    bool m_active = false;
    QTimer *m_timer = nullptr;
    QProcess *m_proc = nullptr;
};
