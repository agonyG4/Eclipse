#pragma once

#include <QObject>

class BarPopupController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool popupOpen READ isOpen NOTIFY changed)
    Q_PROPERTY(PopupKind kind READ kind NOTIFY changed)
    Q_PROPERTY(bool closing READ closing NOTIFY changed)
    Q_PROPERTY(bool surfaceRequired READ surfaceRequired NOTIFY changed)
    Q_PROPERTY(int anchorX READ anchorX NOTIFY changed)
    Q_PROPERTY(QString contextKey READ contextKey NOTIFY changed)

public:
    enum class PopupKind {
        None,
        AstreaMenu,
        Network = 3,
        Bluetooth = 4,
        Volume = 5,
        // Kept for compatibility with the existing Bar popup contract and
        // tests. Production Tray presentation is owned by the global
        // ContextMenuController.
        TrayMenu = 6,
    };
    Q_ENUM(PopupKind)

    explicit BarPopupController(QObject *parent = nullptr);

    bool isOpen() const { return m_open; }
    PopupKind kind() const { return m_kind; }
    bool closing() const { return m_closing; }
    bool surfaceRequired() const { return m_surfaceRequired; }
    int anchorX() const { return m_anchorX; }
    QString contextKey() const { return m_contextKey; }

    Q_INVOKABLE void open(PopupKind kind, int anchorX);
    Q_INVOKABLE void toggle(PopupKind kind, int anchorX);
    Q_INVOKABLE void toggleAstreaMenu(int anchorX);
    Q_INVOKABLE void toggleNetwork(int anchorX);
    Q_INVOKABLE void toggleBluetooth(int anchorX);
    Q_INVOKABLE void toggleVolume(int anchorX);
    Q_INVOKABLE void toggleTrayMenu(int anchorX, const QString &contextKey);
    Q_INVOKABLE void close();
    Q_INVOKABLE void completeClose();
    Q_INVOKABLE void clearForOutput();

signals:
    void changed();

private:
    static bool isSupported(PopupKind kind);
    void openWithContext(PopupKind kind, int anchorX, const QString &contextKey);
    PopupKind m_kind = PopupKind::None;
    bool m_open = false;
    bool m_closing = false;
    bool m_surfaceRequired = false;
    int m_anchorX = 0;
    QString m_contextKey;
};
