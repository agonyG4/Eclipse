#pragma once

#include <QObject>

class BarPopupController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool popupOpen READ isOpen NOTIFY changed)
    Q_PROPERTY(PopupKind kind READ kind NOTIFY changed)
    Q_PROPERTY(bool closing READ closing NOTIFY changed)
    Q_PROPERTY(bool surfaceRequired READ surfaceRequired NOTIFY changed)
    Q_PROPERTY(int anchorX READ anchorX NOTIFY changed)

public:
    enum class PopupKind {
        None,
        AstreaMenu,
        Clock,
    };
    Q_ENUM(PopupKind)

    explicit BarPopupController(QObject *parent = nullptr);

    bool isOpen() const { return m_open; }
    PopupKind kind() const { return m_kind; }
    bool closing() const { return m_closing; }
    bool surfaceRequired() const { return m_surfaceRequired; }
    int anchorX() const { return m_anchorX; }

    Q_INVOKABLE void open(PopupKind kind, int anchorX);
    Q_INVOKABLE void toggle(PopupKind kind, int anchorX);
    Q_INVOKABLE void toggleAstreaMenu(int anchorX);
    Q_INVOKABLE void toggleClock(int anchorX);
    Q_INVOKABLE void close();
    Q_INVOKABLE void completeClose();
    Q_INVOKABLE void clearForOutput();

signals:
    void changed();

private:
    PopupKind m_kind = PopupKind::None;
    bool m_open = false;
    bool m_closing = false;
    bool m_surfaceRequired = false;
    int m_anchorX = 0;
};
