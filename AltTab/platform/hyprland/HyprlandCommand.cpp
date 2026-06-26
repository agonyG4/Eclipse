#include "platform/hyprland/HyprlandCommand.hpp"

#include <QStringList>

namespace {

bool isHexDigits(QStringView value)
{
    if (value.isEmpty())
        return false;
    for (const QChar ch : value) {
        if (!ch.isDigit() && (ch.toLower() < QLatin1Char('a') || ch.toLower() > QLatin1Char('f')))
            return false;
    }
    return true;
}

} // namespace

namespace HyprlandRequest {

QByteArray jsonInfoRequest(QStringView command)
{
    QByteArray result;
    result.reserve(2 + command.size());
    result.append('j');
    result.append('/');
    result.append(command.toUtf8());
    return result;
}

QByteArray evalRequest(QStringView luaExpression)
{
    QByteArray result;
    result.reserve(5 + luaExpression.size());
    result.append("/eval ");
    result.append(luaExpression.toUtf8());
    return result;
}

QByteArray focusWorkspaceRequest(qint64 workspaceId)
{
    if (workspaceId <= 0)
        return {};
    const QString escaped = HyprlandCommand::luaStringLiteral(QString::number(workspaceId));
    const QString expr = QStringLiteral("hl.dispatch(hl.dsp.focus({ workspace = %1 }))").arg(escaped);
    return evalRequest(expr);
}

QByteArray focusWindowRequest(QStringView rawAddress)
{
    const QString norm = HyprlandCommand::normalizeAddress(rawAddress.toString());
    if (norm.isEmpty())
        return {};
    const QString escaped = HyprlandCommand::luaStringLiteral(norm);
    const QString expr = QStringLiteral("hl.dispatch(hl.dsp.focus({ window = %1 }))").arg(escaped);
    return evalRequest(expr);
}

} // namespace HyprlandRequest

namespace HyprlandCommand {

QString luaStringLiteral(const QString &value)
{
    QString out;
    out.reserve(value.size() + 2);
    out.append(QLatin1Char('"'));
    for (const QChar ch : value) {
        switch (ch.unicode()) {
        case u'\\': out.append(QStringLiteral("\\\\")); break;
        case u'"':  out.append(QStringLiteral("\\\"")); break;
        case u'\n': out.append(QStringLiteral("\\n")); break;
        case u'\r': out.append(QStringLiteral("\\r")); break;
        case u'\t': out.append(QStringLiteral("\\t")); break;
        default:
            if (ch.unicode() < 0x20) {
                out.append(QStringLiteral("\\x") + QString::number(ch.unicode(), 16).rightJustified(2, QLatin1Char('0')));
            } else {
                out.append(ch);
            }
            break;
        }
    }
    out.append(QLatin1Char('"'));
    return out;
}

QString normalizeAddress(const QString &address)
{
    QString value = address.trimmed();
    if (value.isEmpty())
        return {};

    // Reject embedded spaces and newlines
    if (value.contains(QLatin1Char(' ')) || value.contains(QLatin1Char('\n')))
        return {};

    value = value.toLower();

    // Strip "address:" prefix
    if (value.startsWith(QStringLiteral("address:"))) {
        // Reject double prefix
        if (value.mid(8).startsWith(QStringLiteral("address:")))
            return {};
        value = value.mid(8);
        if (value.isEmpty())
            return {};
    }

    // Strip "0x" prefix
    if (value.startsWith(QStringLiteral("0x"))) {
        value = value.mid(2);
        if (value.isEmpty())
            return {};
    }

    if (!isHexDigits(value))
        return {};

    return QStringLiteral("address:0x") + value;
}

} // namespace HyprlandCommand
