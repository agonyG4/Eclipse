#include "core/BarPopupController.hpp"

BarPopupController::BarPopupController(QObject *parent)
    : QObject(parent)
{
}

bool BarPopupController::isSupported(PopupKind kind)
{
    switch (kind) {
    case PopupKind::AstreaMenu:
    case PopupKind::Network:
    case PopupKind::Bluetooth:
    case PopupKind::Volume:
        return true;
    case PopupKind::None:
        return false;
    }
    return false;
}

void BarPopupController::open(PopupKind kind, int anchorX)
{
    openWithContext(kind, anchorX, {});
}

void BarPopupController::openWithContext(PopupKind kind, int anchorX,
                                         const QString &contextKey)
{
    if (kind == PopupKind::None) {
        close();
        return;
    }
    if (!isSupported(kind))
        return;
    if (m_kind == kind && m_anchorX == anchorX && m_contextKey == contextKey
        && m_open && !m_closing)
        return;
    m_kind = kind;
    m_open = true;
    m_closing = false;
    m_surfaceRequired = true;
    m_anchorX = anchorX;
    m_contextKey = contextKey;
    emit changed();
}

void BarPopupController::toggle(PopupKind kind, int anchorX)
{
    if (m_kind == kind && m_open && !m_closing) {
        close();
        return;
    }
    openWithContext(kind, anchorX, {});
}

void BarPopupController::toggleAstreaMenu(int anchorX)
{
    toggle(PopupKind::AstreaMenu, anchorX);
}

void BarPopupController::toggleNetwork(int anchorX)
{
    toggle(PopupKind::Network, anchorX);
}

void BarPopupController::toggleBluetooth(int anchorX)
{
    toggle(PopupKind::Bluetooth, anchorX);
}

void BarPopupController::toggleVolume(int anchorX)
{
    toggle(PopupKind::Volume, anchorX);
}

void BarPopupController::close()
{
    if (!m_surfaceRequired || m_closing)
        return;
    m_open = false;
    m_closing = true;
    emit changed();
}

void BarPopupController::completeClose()
{
    if (!m_closing)
        return;
    m_open = false;
    m_closing = false;
    m_surfaceRequired = false;
    m_kind = PopupKind::None;
    m_anchorX = 0;
    m_contextKey.clear();
    emit changed();
}

void BarPopupController::clearForOutput()
{
    if (!m_surfaceRequired && m_kind == PopupKind::None && m_anchorX == 0)
        return;
    m_open = false;
    m_closing = false;
    m_surfaceRequired = false;
    m_kind = PopupKind::None;
    m_anchorX = 0;
    m_contextKey.clear();
    emit changed();
}
