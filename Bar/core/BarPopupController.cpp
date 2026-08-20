#include "core/BarPopupController.hpp"

BarPopupController::BarPopupController(QObject *parent)
    : QObject(parent)
{
}

void BarPopupController::open(PopupKind kind, int anchorX)
{
    if (kind == PopupKind::None) {
        close();
        return;
    }
    if (m_kind == kind && m_anchorX == anchorX && m_open && !m_closing)
        return;
    m_kind = kind;
    m_open = true;
    m_closing = false;
    m_surfaceRequired = true;
    m_anchorX = anchorX;
    emit changed();
}

void BarPopupController::toggle(PopupKind kind, int anchorX)
{
    if (m_kind == kind && m_open && !m_closing) {
        close();
        return;
    }
    open(kind, anchorX);
}

void BarPopupController::toggleAstreaMenu(int anchorX)
{
    toggle(PopupKind::AstreaMenu, anchorX);
}

void BarPopupController::toggleClock(int anchorX)
{
    toggle(PopupKind::Clock, anchorX);
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
    emit changed();
}
