#pragma once

#include <QQuickWindow>

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

    bool effectAvailable() const { return m_effectAvailable; }
    bool effectActive() const { return m_effectActive; }
    bool effectEnabled() const { return m_effectEnabled; }
    qreal cornerRadius() const { return m_cornerRadius; }
    void setEffectEnabled(bool enabled);
    void setCornerRadius(qreal radius);

signals:
    void effectStateChanged();

private:
    void scheduleEffectUpdate();
    void updateEffect();

    bool m_effectAvailable = false;
    bool m_effectActive = false;
    bool m_effectEnabled = true;
    qreal m_cornerRadius = 18.0;
};
