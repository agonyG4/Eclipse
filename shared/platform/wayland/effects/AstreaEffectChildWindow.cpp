#include "platform/wayland/effects/AstreaEffectChildWindow.hpp"

#include "platform/wayland/effects/AstreaWaylandEffects.hpp"

#include <QEvent>
#include <QMetaObject>
#include <QPlatformSurfaceEvent>
#include <QSurfaceFormat>
#include <qpa/qplatformwindow.h>

AstreaEffectChildWindow::AstreaEffectChildWindow(QWindow *parent)
    : QQuickWindow(parent)
    , m_controller(std::make_unique<AstreaEffectSurfaceController>(this))
{
    QSurfaceFormat format = requestedFormat();
    format.setAlphaBufferSize(8);
    setFormat(format);
    setColor(Qt::transparent);
    setFlags(Qt::FramelessWindowHint | Qt::WindowTransparentForInput
             | Qt::NoDropShadowWindowHint);
    connect(this, &QWindow::visibleChanged, this, &AstreaEffectChildWindow::scheduleEffectUpdate);
    connect(this, &QWindow::widthChanged, this, &AstreaEffectChildWindow::scheduleEffectUpdate);
    connect(this, &QWindow::heightChanged, this, &AstreaEffectChildWindow::scheduleEffectUpdate);
    connect(this, &QWindow::screenChanged, this, &AstreaEffectChildWindow::scheduleEffectUpdate);
}

AstreaEffectChildWindow::~AstreaEffectChildWindow()
{
    QObject::disconnect(this, nullptr, this, nullptr);
}

bool AstreaEffectChildWindow::effectAvailable() const
{
    return m_controller && m_controller->available();
}

bool AstreaEffectChildWindow::effectActive() const
{
    return m_controller && m_controller->active();
}

bool AstreaEffectChildWindow::effectEnabled() const
{
    return m_controller && m_controller->enabled();
}

qreal AstreaEffectChildWindow::cornerRadius() const
{
    return m_controller ? m_controller->cornerRadius() : 0.0;
}

void AstreaEffectChildWindow::setEffectEnabled(bool enabled)
{
    if (!m_controller || m_controller->enabled() == enabled)
        return;
    m_controller->setEnabled(enabled);
    emit effectStateChanged();
    scheduleEffectUpdate();
}

void AstreaEffectChildWindow::setCornerRadius(qreal radius)
{
    const qreal bounded = qMax<qreal>(0.0, radius);
    if (!m_controller || qFuzzyCompare(m_controller->cornerRadius(), bounded))
        return;
    m_controller->setCornerRadius(bounded);
    emit effectStateChanged();
    scheduleEffectUpdate();
}

bool AstreaEffectChildWindow::event(QEvent *event)
{
    const auto *surfaceEvent = event && event->type() == QEvent::PlatformSurface
        ? static_cast<const QPlatformSurfaceEvent *>(event)
        : nullptr;
    if (surfaceEvent
        && surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
        const bool wasActive = effectActive();
        m_controller->surfaceAboutToBeDestroyed();
        if (wasActive || effectAvailable())
            emit effectStateChanged();
    }

    const bool handled = QQuickWindow::event(event);
    if (surfaceEvent
        && surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated)
        scheduleEffectUpdate();
    // Reparenting a mapped child makes Qt replace its wl_subsurface without
    // sending the position again, and a new subsurface starts at (0, 0). The
    // QWindow geometry did not change, so only the platform window can resend it.
    if (event && event->type() == QEvent::ParentWindowChange && parent() && handle())
        handle()->setGeometry(geometry());
    return handled;
}

void AstreaEffectChildWindow::scheduleEffectUpdate()
{
    QMetaObject::invokeMethod(this, [this] { updateEffect(); }, Qt::QueuedConnection);
}

void AstreaEffectChildWindow::updateEffect()
{
    auto *effects = AstreaWaylandEffects::instance();
    if (effects)
        connect(effects, &AstreaWaylandEffects::availableChanged,
                this, &AstreaEffectChildWindow::scheduleEffectUpdate, Qt::UniqueConnection);

    const bool wasAvailable = effectAvailable();
    const bool wasActive = effectActive();
    m_controller->sync();
    if (wasAvailable != effectAvailable() || wasActive != effectActive())
        emit effectStateChanged();
}
