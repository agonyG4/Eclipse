#pragma once

#include <QByteArray>
#include <QString>

namespace HyprlandRequest {

// Wire format (verified against Hyprland 0.55 hyprctl source):
//   Info requests:  j/<command>
//   Eval requests:  /eval <lua>
// No trailing newline, no spaces.
QByteArray jsonInfoRequest(QStringView command);
QByteArray evalRequest(QStringView luaExpression);
QByteArray focusWorkspaceRequest(qint64 workspaceId);
QByteArray focusWindowRequest(QStringView rawAddress);

}

namespace HyprlandCommand {

// Normalize a window address to "address:0x<hex>" format.
// Accepts: "1234", "0x1234", "address:1234", "address:0x1234"
// Rejects: empty, "0x", "address:", "address:0x", non-hex, spaces, newlines
QString normalizeAddress(const QString &address);

// Safe Lua string-literal encoder.
QString luaStringLiteral(const QString &value);

}
