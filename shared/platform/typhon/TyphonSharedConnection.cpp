#include "platform/typhon/TyphonSharedConnection.hpp"

#include "platform/typhon/TyphonShellAuthenticator.hpp"
#include "platform/typhon/TyphonWaylandDisplay.hpp"

#include <QDebug>

struct TyphonSharedConnection::Private {
    std::unique_ptr<TyphonWaylandDisplay> display;
};

TyphonSharedConnection::TyphonSharedConnection(Hooks hooks, QObject *parent)
    : QObject(parent), m_private(std::make_unique<Private>()), m_hooks(std::move(hooks))
{
    m_private->display = std::make_unique<TyphonWaylandDisplay>(this);
    connect(m_private->display.get(), &TyphonWaylandDisplay::disconnected,
            this, &TyphonSharedConnection::handleDisconnected);
    connect(m_private->display.get(), &TyphonWaylandDisplay::protocolError,
            this, &TyphonSharedConnection::handleProtocolError);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this] {
        if (m_started)
            beginConnection();
    });

    if (m_hooks.setDisconnectHandler)
        m_hooks.setDisconnectHandler([this] { handleDisconnected(); });
}

TyphonSharedConnection::~TyphonSharedConnection()
{
    stop();
}

void TyphonSharedConnection::start()
{
    if (m_started)
        return;

    m_started = true;
    m_backoffIndex = 0;
    m_reconnectTimer.stop();
    beginConnection();
}

void TyphonSharedConnection::stop()
{
    if (!m_started && m_state == State::Stopped)
        return;

    m_started = false;
    m_reconnectTimer.stop();
    m_failureInProgress = true;
    disconnectDisplay();
    m_failureInProgress = false;
    setState(State::Stopped);
}

wl_display *TyphonSharedConnection::nativeDisplay() const
{
    return m_private->display ? m_private->display->nativeDisplay() : nullptr;
}

void TyphonSharedConnection::reconnectNowForTest()
{
    if (!m_started)
        return;
    m_reconnectTimer.stop();
    beginConnection();
}

void TyphonSharedConnection::beginConnection()
{
    if (!m_started)
        return;

    ++m_generation;
    setState(State::Connecting);

    if (!connectDisplay()) {
        fail(QStringLiteral("Typhon shared shell could not connect to Wayland"));
        return;
    }

    setState(State::Authenticating);
    QString diagnostic;
    if (!authenticate(&diagnostic)) {
        fail(diagnostic.isEmpty() ? QStringLiteral("Typhon shared shell authentication failed")
                                  : diagnostic);
        return;
    }

    m_authenticationGeneration = m_generation;
    m_backoffIndex = 0;
    setState(State::Ready);
    emit ready(m_generation);
}

void TyphonSharedConnection::handleDisconnected()
{
    if (!m_started || m_failureInProgress)
        return;

    setState(State::Disconnected);
    emit disconnected(m_generation);
    scheduleReconnect();
}

void TyphonSharedConnection::handleProtocolError(const QString &message)
{
    if (!m_started || m_failureInProgress)
        return;
    fail(message);
}

void TyphonSharedConnection::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void TyphonSharedConnection::fail(const QString &message)
{
    if (!m_started)
        return;

    emit diagnostic(message);
    qWarning("Typhon shared shell connection degraded: %s", qPrintable(message));
    m_failureInProgress = true;
    disconnectDisplay();
    m_failureInProgress = false;
    setState(State::Degraded);
    emit disconnected(m_generation);
    scheduleReconnect();
}

void TyphonSharedConnection::scheduleReconnect()
{
    if (!m_started || m_reconnectTimer.isActive())
        return;
    m_reconnectTimer.start(reconnectDelay());
    m_backoffIndex = qMin(m_backoffIndex + 1, 4);
}

int TyphonSharedConnection::reconnectDelay() const
{
    static constexpr int delays[] = {250, 500, 1000, 2000, 5000};
    return delays[qBound(0, m_backoffIndex, 4)];
}

bool TyphonSharedConnection::connectDisplay()
{
    if (m_hooks.connect)
        return m_hooks.connect();
    return m_private->display && m_private->display->connectToDisplay();
}

bool TyphonSharedConnection::authenticate(QString *diagnostic)
{
    if (m_hooks.authenticate)
        return m_hooks.authenticate(diagnostic);
    return Astrea::Typhon::TyphonShellAuthenticator::authenticate(nativeDisplay(), diagnostic);
}

void TyphonSharedConnection::disconnectDisplay()
{
    if (m_hooks.disconnect) {
        m_hooks.disconnect();
        return;
    }
    if (m_private->display)
        m_private->display->disconnectFromDisplay();
}
