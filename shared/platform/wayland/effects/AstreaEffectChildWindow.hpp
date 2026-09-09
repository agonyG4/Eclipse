#pragma once

#include "platform/wayland/effects/AstreaEffectSurfaceController.hpp"

#include <QQuickWindow>

#include <memory>

#include <QtQml/qqmlregistration.h>

class AstreaEffectChildWindow : public QQuickWindow {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool effectAvailable READ effectAvailable NOTIFY effectStateChanged)
    Q_PROPERTY(bool effectActive READ effectActive NOTIFY effectStateChanged)
    Q_PROPERTY(bool effectEnabled READ effectEnabled WRITE setEffectEnabled NOTIFY effectStateChanged)
    Q_PROPERTY(qreal cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY effectStateChanged)

public:
    explicit AstreaEffectChildWindow(QWindow *parent = nullptr);
    ~AstreaEffectChildWindow() override;

    bool effectAvailable() const;
    bool effectActive() const;
    bool effectEnabled() const;
    qreal cornerRadius() const;
    void setEffectEnabled(bool enabled);
    void setCornerRadius(qreal radius);

signals:
    void effectStateChanged();

private:
    bool event(QEvent *event) override;
    void scheduleEffectUpdate();
    void updateEffect();

    std::unique_ptr<AstreaEffectSurfaceController> m_controller;
};
