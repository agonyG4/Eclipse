#include "ContextMenuController.hpp"

#include "ContextMenuProviders.hpp"
#include "ContextMenuPlacement.hpp"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

#include <utility>

namespace {

bool contextMenuDebugEnabled()
{
    return qgetenv("ASTREA_CONTEXT_MENU_DEBUG") == QByteArrayLiteral("1");
}

void logContextMenuDebug(const QString &event, const QVariantMap &fields)
{
    if (!contextMenuDebugEnabled())
        return;
    qInfo().noquote() << QStringLiteral("ASTREA_CONTEXT_MENU_DEBUG event=%1 data=%2")
                             .arg(event,
                                  QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(fields))
                                                       .toJson(QJsonDocument::Compact)));
}

} // namespace

namespace Astrea::Shell {

ContextMenuController::ContextMenuController(QObject *parent)
    : QObject(parent)
    , m_model(std::make_unique<ContextMenuModel>(this))
{
}

bool ContextMenuController::debugEnabled() const
{
    return qgetenv("ASTREA_CONTEXT_MENU_DEBUG") == QByteArrayLiteral("1");
}

bool ContextMenuController::present(const ContextMenuTarget &target,
                                    const QVector<ContextMenuModel::NodeSpec> &nodes,
                                    ActivationHandler activation,
                                    TargetValidator targetValidator,
                                    ActionAuthorizer actionAuthorizer)
{
    return present(target, ContextMenuAnchor{}, nodes, std::move(activation),
                   std::move(targetValidator), std::move(actionAuthorizer));
}

bool ContextMenuController::present(const ContextMenuTarget &target,
                                    const ContextMenuAnchor &anchor,
                                    const QVector<ContextMenuModel::NodeSpec> &nodes,
                                    ActivationHandler activation,
                                    TargetValidator targetValidator,
                                    ActionAuthorizer actionAuthorizer)
{
    auto candidate = std::make_unique<ContextMenuModel>();
    if (!candidate->setRootNodes(nodes)) {
        return false;
    }

    if (m_barPopupCloser)
        m_barPopupCloser();

    ++m_presentationGeneration;
    m_target = target;
    m_anchor = anchor;
    if (target.kind != ContextMenuTarget::Kind::TrayItem)
        m_trayService.clear();
    m_targetValid = true;
    m_activation = std::move(activation);
    m_targetValidator = std::move(targetValidator);
    m_actionAuthorizer = std::move(actionAuthorizer);
    m_model->setRootNodes(nodes);

    logContextMenuDebug(QStringLiteral("presentation"), {
        {QStringLiteral("targetKind"), static_cast<int>(target.kind)},
        {QStringLiteral("targetIdentity"), target.identity},
        {QStringLiteral("outputKey"), target.outputKey},
        {QStringLiteral("generation"), QVariant::fromValue(m_presentationGeneration)},
        {QStringLiteral("anchorKind"), static_cast<int>(anchor.kind)},
        {QStringLiteral("anchorPointX"), anchor.point.x()},
        {QStringLiteral("anchorPointY"), anchor.point.y()},
        {QStringLiteral("anchorX"), anchor.rectangle.x()},
        {QStringLiteral("anchorY"), anchor.rectangle.y()},
        {QStringLiteral("anchorWidth"), anchor.rectangle.width()},
        {QStringLiteral("anchorHeight"), anchor.rectangle.height()},
    });

    const Lifecycle previous = m_lifecycle;
    m_lifecycle = Lifecycle::Open;
    if (previous != m_lifecycle) {
        emit lifecycleChanged();
    }
    emit presentationChanged();
    return true;
}

bool ContextMenuController::presentDesktop(int x, int y, const QString &outputKey)
{
    logContextMenuDebug(QStringLiteral("desktop-anchor-input"), {
        {QStringLiteral("outputKey"), outputKey},
        {QStringLiteral("anchorPointX"), x},
        {QStringLiteral("anchorPointY"), y},
    });
    if (!m_desktopProvider)
        return false;
    return m_desktopProvider->present(this, QPoint(x, y), outputKey);
}

bool ContextMenuController::presentDock(const QString &desktopFileName, int x, int y,
                                        int width, int height, const QString &outputKey)
{
    logContextMenuDebug(QStringLiteral("dock-anchor-input"), {
        {QStringLiteral("targetIdentity"), desktopFileName},
        {QStringLiteral("outputKey"), outputKey},
        {QStringLiteral("anchorX"), x},
        {QStringLiteral("anchorY"), y},
        {QStringLiteral("anchorWidth"), width},
        {QStringLiteral("anchorHeight"), height},
    });
    if (!m_dockProvider)
        return false;
    return m_dockProvider->present(this, desktopFileName, QRect(x, y, width, height), outputKey);
}

bool ContextMenuController::presentTray(const QString &itemKey, int x, int y, int width,
                                        int height, int preferredTop, const QString &outputKey)
{
    if (!m_trayProvider)
        return false;
    ContextMenuAnchor anchor;
    anchor.kind = ContextMenuAnchor::Kind::Rectangle;
    anchor.rectangle = QRect(x, y, qMax(0, width), qMax(0, height));
    anchor.preferredTop = preferredTop;
    return m_trayProvider->present(this, itemKey, anchor, outputKey);
}

QPoint ContextMenuController::menuPosition(int outputWidth, int outputHeight,
                                            int menuWidth, int menuHeight) const
{
    ContextMenuPlacement::Request request;
    request.output = QRect(0, 0, qMax(1, outputWidth), qMax(1, outputHeight));
    request.menuSize = QSize(menuWidth, menuHeight);
    request.kind = m_anchor.kind == ContextMenuAnchor::Kind::Rectangle
        ? (m_target.kind == ContextMenuTarget::Kind::DockApplication
               ? ContextMenuPlacement::Kind::Dock : ContextMenuPlacement::Kind::Rectangle)
        : ContextMenuPlacement::Kind::Point;
    request.anchor = m_anchor.point;
    request.sourceRect = m_anchor.rectangle;
    request.preferredTop = m_anchor.preferredTop;
    if (m_anchor.kind == ContextMenuAnchor::Kind::Rectangle
        && m_target.kind == ContextMenuTarget::Kind::TrayItem)
        request.kind = ContextMenuPlacement::Kind::CenteredRectangle;
    return ContextMenuPlacement::place(request).position;
}

QPoint ContextMenuController::submenuPosition(int outputWidth, int outputHeight,
                                               int menuWidth, int menuHeight,
                                               const QRect &parentRectangle,
                                               bool rightToLeft) const
{
    ContextMenuPlacement::Request request;
    request.output = QRect(0, 0, qMax(1, outputWidth), qMax(1, outputHeight));
    request.menuSize = QSize(menuWidth, menuHeight);
    request.kind = ContextMenuPlacement::Kind::Submenu;
    request.parentRect = parentRectangle;
    request.direction = rightToLeft ? Qt::RightToLeft : Qt::LeftToRight;
    return ContextMenuPlacement::place(request).position;
}

void ContextMenuController::invalidateOutput(const QString &outputKey)
{
    if (!hasActivePresentation() || m_target.outputKey != outputKey)
        return;
    invalidateTarget();
    // An output being destroyed cannot host the exit animation. Settle the
    // lifecycle before its surface bundle is deleted.
    completeClose();
}

bool ContextMenuController::activate(quint64 generation, const QString &token)
{
    if (m_lifecycle != Lifecycle::Open || generation != m_presentationGeneration) {
        emit activationRejected(token);
        return false;
    }
    if (!m_targetValid || (m_targetValidator && !m_targetValidator())) {
        m_targetValid = false;
        close();
        emit activationRejected(token);
        return false;
    }
    const bool authorized = m_actionAuthorizer ? m_actionAuthorizer(token)
                                                : m_model->canActivate(token);
    if (!authorized || !m_activation) {
        emit activationRejected(token);
        return false;
    }

    if (!m_activation(token)) {
        emit activationRejected(token);
        return false;
    }
    close();
    return true;
}

void ContextMenuController::close()
{
    if (m_lifecycle == Lifecycle::Closed || m_lifecycle == Lifecycle::Closing) {
        return;
    }
    m_lifecycle = Lifecycle::Closing;
    emit lifecycleChanged();
    emit closeRequested();
}

void ContextMenuController::completeClose()
{
    if (m_lifecycle == Lifecycle::Closed) {
        return;
    }
    m_lifecycle = Lifecycle::Closed;
    m_targetValid = false;
    m_activation = {};
    m_targetValidator = {};
    m_actionAuthorizer = {};
    m_trayService.clear();
    m_model->clear();
    m_target = {};
    m_anchor = {};
    emit lifecycleChanged();
    emit presentationChanged();
}

void ContextMenuController::invalidateTarget()
{
    if (!hasActivePresentation()) {
        return;
    }
    m_targetValid = false;
    close();
}

void ContextMenuController::invalidateProvider()
{
    invalidateTarget();
}

void ContextMenuController::shutdown()
{
    close();
    completeClose();
}

} // namespace Astrea::Shell
