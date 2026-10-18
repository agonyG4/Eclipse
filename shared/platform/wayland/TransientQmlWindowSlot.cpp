#include "platform/wayland/TransientQmlWindowSlot.hpp"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QThread>
#include <QTimer>

TransientQmlWindowSlot::TransientQmlWindowSlot(QString diagnosticId, QObject *parent)
    : QObject(parent), m_diagnosticId(std::move(diagnosticId))
{
}

TransientQmlWindowSlot::~TransientQmlWindowSlot()
{
    destroyNow();
}

QQuickWindow *TransientQmlWindowSlot::window() const
{
    return m_window.data();
}

bool TransientQmlWindowSlot::created() const
{
    return !m_window.isNull();
}

QQuickWindow *TransientQmlWindowSlot::ensureCreated(QQmlEngine *engine,
                                                    const CreateRequest &request,
                                                    QString *errorOut)
{
    if (!onOwnerThread("create")) {
        if (errorOut)
            *errorOut = QStringLiteral("Transient QML windows must be created on their owner thread");
        return nullptr;
    }
    if (m_window) {
        if (m_destroyPending) {
            ++m_generation;
            m_destroyPending = false;
            emit residencyChanged();
        }
        return m_window.data();
    }
    if (!engine || !request.screen) {
        const QString reason = QStringLiteral("Transient QML window requires an engine and screen");
        if (errorOut)
            *errorOut = reason;
        return nullptr;
    }

    QQmlComponent component(engine, this);
    component.loadUrl(request.sourceUrl, QQmlComponent::PreferSynchronous);
    if (component.status() != QQmlComponent::Ready) {
        const QString reason = component.errors().isEmpty()
            ? QStringLiteral("Transient QML component is not ready")
            : component.errors().constFirst().toString();
        if (errorOut)
            *errorOut = reason;
        return nullptr;
    }

    QVariantMap properties = request.initialProperties;
    if (request.requireSurfaceReadyGate)
        properties.insert(QStringLiteral("surfaceReady"), false);
    QObject *object = component.createWithInitialProperties(properties, engine->rootContext());
    if (!object) {
        const QString reason = component.errors().isEmpty()
            ? QStringLiteral("Transient QML component did not create an object")
            : component.errors().constFirst().toString();
        if (errorOut)
            *errorOut = reason;
        return nullptr;
    }

    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        delete object;
        const QString reason = QStringLiteral("Transient QML component did not create a QQuickWindow");
        if (errorOut)
            *errorOut = reason;
        return nullptr;
    }

    QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
    window->setScreen(request.screen);
    if (request.size.isValid() && !request.size.isEmpty())
        window->resize(request.size);
    window->setPersistentGraphics(false);
    window->setPersistentSceneGraph(false);

    if (request.requireSurfaceReadyGate
        && window->metaObject()->indexOfProperty("surfaceReady") < 0) {
        const QString reason = QStringLiteral("Transient QML window is missing surfaceReady gate");
        delete window;
        if (errorOut)
            *errorOut = reason;
        return nullptr;
    }
    if (window->isVisible()) {
        const QString reason = QStringLiteral(
            "Transient QML window must remain hidden until configuration succeeds");
        delete window;
        if (errorOut)
            *errorOut = reason;
        return nullptr;
    }

    m_window = window;
    m_beforeDestroy = request.beforeDestroy;
    ++m_createCount;
    connect(window, &QWindow::visibleChanged, this, &TransientQmlWindowSlot::residencyChanged);
    connect(window, &QWindow::widthChanged, this, &TransientQmlWindowSlot::residencyChanged);
    connect(window, &QWindow::heightChanged, this, &TransientQmlWindowSlot::residencyChanged);
    connect(window, &QObject::destroyed, this, [this, window] {
        if (m_window.data() == window)
            m_window.clear();
        m_destroyPending = false;
        ++m_destroyCount;
        m_beforeDestroy = {};
        emit residencyChanged();
        emit windowDestroyed();
    });
    emit residencyChanged();

    QString reason;
    if (request.configure && !request.configure(window, &reason)) {
        failCreation(window, reason.isEmpty()
            ? QStringLiteral("Transient QML window configuration failed") : reason, errorOut);
        return nullptr;
    }
    if (request.install && !request.install(window, &reason)) {
        failCreation(window, reason.isEmpty()
            ? QStringLiteral("Transient QML window installation failed") : reason, errorOut);
        return nullptr;
    }
    if (request.requireSurfaceReadyGate
        && !window->setProperty("surfaceReady", true)) {
        failCreation(window, QStringLiteral("Transient QML surfaceReady gate could not be opened"),
                     errorOut);
        return nullptr;
    }

    ++m_generation;
    emit residencyChanged();
    return window;
}

QVariantMap TransientQmlWindowSlot::residencySnapshot() const
{
    const bool live = m_window;
    const bool visible = live && m_window->isVisible();
    return {
        {QStringLiteral("class"), QStringLiteral("transient")},
        {QStringLiteral("created"), live},
        {QStringLiteral("visible"), visible},
        {QStringLiteral("destroyPending"), m_destroyPending},
        {QStringLiteral("width"), live ? m_window->width() : 0},
        {QStringLiteral("height"), live ? m_window->height() : 0},
        {QStringLiteral("liveInstances"), live ? 1 : 0},
        {QStringLiteral("visibleInstances"), visible ? 1 : 0},
        {QStringLiteral("createCount"), QVariant::fromValue<qulonglong>(m_createCount)},
        {QStringLiteral("destroyCount"), QVariant::fromValue<qulonglong>(m_destroyCount)},
    };
}

void TransientQmlWindowSlot::hide()
{
    if (!onOwnerThread("hide")) {
        QMetaObject::invokeMethod(this, [this] { hide(); }, Qt::QueuedConnection);
        return;
    }
    if (m_window)
        m_window->hide();
}

void TransientQmlWindowSlot::destroyWhenSafe()
{
    if (!onOwnerThread("schedule destruction")) {
        QMetaObject::invokeMethod(this, [this] { destroyWhenSafe(); }, Qt::QueuedConnection);
        return;
    }
    if (!m_window)
        return;
    if (m_destroyPending)
        return;
    m_window->hide();
    const quint64 generation = ++m_generation;
    m_destroyPending = true;
    QTimer::singleShot(0, this, [this, generation] {
        if (generation == m_generation)
            destroyNow();
    });
    emit residencyChanged();
}

void TransientQmlWindowSlot::cancelPendingDestruction()
{
    if (!onOwnerThread("cancel destruction")) {
        QMetaObject::invokeMethod(this, [this] { cancelPendingDestruction(); },
                                  Qt::QueuedConnection);
        return;
    }
    if (!m_window || !m_destroyPending)
        return;
    ++m_generation;
    m_destroyPending = false;
    emit residencyChanged();
}

void TransientQmlWindowSlot::destroyNow()
{
    if (!onOwnerThread("destroy")) {
        QMetaObject::invokeMethod(this, [this] { destroyNow(); }, Qt::QueuedConnection);
        return;
    }
    ++m_generation;
    m_destroyPending = false;
    QQuickWindow *window = m_window.data();
    if (!window)
        return;
    window->hide();
    if (m_beforeDestroy)
        m_beforeDestroy(window);
    delete window;
}

bool TransientQmlWindowSlot::onOwnerThread(const char *operation)
{
    if (QThread::currentThread() == thread())
        return true;
    qWarning("Transient QML window %s requested from the wrong thread", operation);
    return false;
}

void TransientQmlWindowSlot::failCreation(QQuickWindow *window, const QString &reason,
                                          QString *errorOut)
{
    if (errorOut)
        *errorOut = reason;
    if (window && window == m_window.data())
        destroyNow();
    else if (window)
        delete window;
}
