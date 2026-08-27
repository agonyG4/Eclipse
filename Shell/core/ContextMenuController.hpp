#pragma once

#include "ContextMenuModel.hpp"

#include <QObject>
#include <QString>

#include <functional>

namespace Astrea::Shell {

struct ContextMenuTarget {
    enum class Kind {
        Desktop,
        DockApplication,
        TrayItem,
    };

    Kind kind = Kind::Desktop;
    QString identity;
    QString outputKey;
};

class ContextMenuController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(Lifecycle lifecycle READ lifecycle NOTIFY lifecycleChanged)
    Q_PROPERTY(quint64 presentationGeneration READ presentationGeneration NOTIFY presentationChanged)
    Q_PROPERTY(bool hasActivePresentation READ hasActivePresentation NOTIFY presentationChanged)
    Q_PROPERTY(ContextMenuModel *model READ model CONSTANT)
    Q_PROPERTY(QString targetIdentity READ targetIdentity NOTIFY presentationChanged)

public:
    enum class Lifecycle {
        Closed,
        Open,
        Closing,
    };
    Q_ENUM(Lifecycle)

    using ActivationHandler = std::function<bool(const QString &token)>;
    using TargetValidator = std::function<bool()>;

    explicit ContextMenuController(QObject *parent = nullptr);

    Lifecycle lifecycle() const { return m_lifecycle; }
    quint64 presentationGeneration() const { return m_presentationGeneration; }
    bool hasActivePresentation() const { return m_lifecycle != Lifecycle::Closed; }
    ContextMenuModel *model() const { return m_model.get(); }
    QString targetIdentity() const { return m_target.identity; }
    const ContextMenuTarget &target() const { return m_target; }

    bool present(const ContextMenuTarget &target,
                 const QVector<ContextMenuModel::NodeSpec> &nodes,
                 ActivationHandler activation,
                 TargetValidator targetValidator = {});
    Q_INVOKABLE bool activate(quint64 generation, const QString &token);
    Q_INVOKABLE void close();
    Q_INVOKABLE void completeClose();
    void invalidateTarget();
    void invalidateProvider();
    void shutdown();

signals:
    void lifecycleChanged();
    void presentationChanged();
    void closeRequested();
    void activationRejected(const QString &token);

private:
    Lifecycle m_lifecycle = Lifecycle::Closed;
    quint64 m_presentationGeneration = 0;
    ContextMenuTarget m_target;
    bool m_targetValid = false;
    std::unique_ptr<ContextMenuModel> m_model;
    ActivationHandler m_activation;
    TargetValidator m_targetValidator;
};

} // namespace Astrea::Shell
