#include "ContextMenuController.hpp"

namespace Astrea::Shell {

ContextMenuController::ContextMenuController(QObject *parent)
    : QObject(parent)
    , m_model(std::make_unique<ContextMenuModel>(this))
{
}

bool ContextMenuController::present(const ContextMenuTarget &target,
                                    const QVector<ContextMenuModel::NodeSpec> &nodes,
                                    ActivationHandler activation,
                                    TargetValidator targetValidator)
{
    return present(target, ContextMenuAnchor{}, nodes, std::move(activation),
                   std::move(targetValidator));
}

bool ContextMenuController::present(const ContextMenuTarget &target,
                                    const ContextMenuAnchor &anchor,
                                    const QVector<ContextMenuModel::NodeSpec> &nodes,
                                    ActivationHandler activation,
                                    TargetValidator targetValidator)
{
    auto candidate = std::make_unique<ContextMenuModel>();
    if (!candidate->setRootNodes(nodes)) {
        return false;
    }

    ++m_presentationGeneration;
    m_target = target;
    m_anchor = anchor;
    m_targetValid = true;
    m_activation = std::move(activation);
    m_targetValidator = std::move(targetValidator);
    m_model->setRootNodes(nodes);

    const Lifecycle previous = m_lifecycle;
    m_lifecycle = Lifecycle::Open;
    if (previous != m_lifecycle) {
        emit lifecycleChanged();
    }
    emit presentationChanged();
    return true;
}

bool ContextMenuController::activate(quint64 generation, const QString &token)
{
    if (m_lifecycle != Lifecycle::Open || generation != m_presentationGeneration
        || !m_targetValid || (m_targetValidator && !m_targetValidator())
        || !m_model->canActivate(token) || !m_activation) {
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
    m_model->clear();
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
