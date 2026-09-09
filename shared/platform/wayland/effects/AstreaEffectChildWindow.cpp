#include "platform/wayland/effects/AstreaEffectChildWindow.hpp"

#include "platform/wayland/effects/AstreaWaylandEffects.hpp"
#include "platform/wayland/effects/RoundedEffectRegion.hpp"

#include <QMetaObject>
#include <QTimer>

AstreaEffectChildWindow::AstreaEffectChildWindow(QWindow *parent)
    : QQuickWindow(parent)
{
    setColor(Qt::transparent);
    setFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::NoDropShadowWindowHint);
    connect(this, &QWindow::visibleChanged, this, &AstreaEffectChildWindow::scheduleEffectUpdate);
    connect(this, &QWindow::widthChanged, this, &AstreaEffectChildWindow::scheduleEffectUpdate);
    connect(this, &QWindow::heightChanged, this, &AstreaEffectChildWindow::scheduleEffectUpdate);
    connect(this, &QWindow::screenChanged, this, &AstreaEffectChildWindow::scheduleEffectUpdate);
}

AstreaEffectChildWindow::~AstreaEffectChildWindow()
{
    if (auto *effects = AstreaWaylandEffects::instance())
        effects->clear(this);
}

void AstreaEffectChildWindow::setEffectEnabled(bool enabled)
{
    if (m_effectEnabled == enabled)
        return;
    m_effectEnabled = enabled;
    emit effectStateChanged();
    scheduleEffectUpdate();
}

void AstreaEffectChildWindow::setCornerRadius(qreal radius)
{
    const qreal bounded = qMax<qreal>(0.0, radius);
    if (qFuzzyCompare(m_cornerRadius, bounded))
        return;
    m_cornerRadius = bounded;
    emit effectStateChanged();
    scheduleEffectUpdate();
}

void AstreaEffectChildWindow::scheduleEffectUpdate()
{
    QMetaObject::invokeMethod(this, &AstreaEffectChildWindow::updateEffect,
                               Qt::QueuedConnection);
}

void AstreaEffectChildWindow::updateEffect()
{
    auto *effects = AstreaWaylandEffects::instance();
    const bool available = effects && effects->initialize();
    bool active = false;
    if (available && m_effectEnabled && isVisible() && width() > 0 && height() > 0) {
        active = effects->setBlurRegion(
            this, AstreaRoundedEffectRegion::rectangles(size(), m_cornerRadius));
    } else if (effects && (!m_effectEnabled || !isVisible())) {
        effects->clear(this);
    }

    if (m_effectAvailable != available || m_effectActive != active) {
        m_effectAvailable = available;
        m_effectActive = active;
        emit effectStateChanged();
    }
}
