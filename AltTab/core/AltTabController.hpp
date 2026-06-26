#pragma once

#include "core/AltTabWindowModel.hpp"
#include "platform/compositor/CompositorBackend.hpp"
#include "services/AppIdentityResolver.hpp"

#include <QBasicTimer>
#include <QObject>
#include <QTimer>
#include <memory>

class AltTabController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(bool surfaceVisible READ surfaceVisible NOTIFY surfaceVisibleChanged)
    Q_PROPERTY(AltTabWindowModel *windowModel READ windowModel CONSTANT)
    Q_PROPERTY(int windowCount READ windowCount NOTIFY windowCountChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex NOTIFY selectedIndexChanged)
    Q_PROPERTY(QString stateName READ stateName NOTIFY stateChanged)

public:
    enum class State {
        Hidden,
        Opening,
        Open,
        Committing,
        Closing
    };
    Q_ENUM(State)

    explicit AltTabController(CompositorBackend *backend, AppIdentityResolver *identityResolver,
                              QObject *parent = nullptr);

    State state() const { return m_state; }
    QString stateName() const;
    bool isOpen() const { return m_state == State::Open || m_state == State::Opening; }
    bool surfaceVisible() const { return m_surfaceVisible; }
    AltTabWindowModel *windowModel() { return &m_model; }
    int windowCount() const { return m_model.count(); }
    int selectedIndex() const { return m_selectedIndex; }

    Q_INVOKABLE void step(int direction);
    Q_INVOKABLE void preview(int index);
    Q_INVOKABLE void commit();
    Q_INVOKABLE void commitIndex(int index);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();
    Q_INVOKABLE void reloadWindows();

    void setSelectedIndex(int index);

    QString buildStatusJson() const;

signals:
    void openChanged();
    void surfaceVisibleChanged();
    void windowCountChanged();
    void selectedIndexChanged();
    void stateChanged();
    void focusRequested();
    void closed();

private slots:
    void onSnapshotReady(RequestToken token, const WindowSnapshot &snapshot);
    void onIdentityResolved(const QString &address, const AppIdentity &identity);
    void onActivationFinished(ActivationToken token, ActivationResult result);
    void onBackendStateChanged(BackendState state);

private:
    void setState(State s);
    void resolveIconsForWindows(const QVector<WindowInfo> &windows);
    void finishOpening();
    void doCommit();
    void onCommitComplete(bool success, const QString &error);
    void timerEvent(QTimerEvent *event) override;
    void cleanup();

    State m_state = State::Hidden;
    bool m_surfaceVisible = false;
    int m_selectedIndex = -1;
    int m_pendingOpenOffset = 0;
    bool m_commitAfterLoad = false;
    bool m_commitInFlight = false;

    quint64 m_openingGeneration = 0;
    quint64 m_timerGeneration = 0;
    quint64 m_activationRequestGen = 0;
    ActivationResult m_lastActivationResult;

    AltTabWindowModel m_model;
    CompositorBackend *m_backend;
    AppIdentityResolver *m_identityResolver;
    QBasicTimer m_snapshotTimeout;
};
