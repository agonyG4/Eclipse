#include "core/WindowInfo.hpp"
#include <QRegularExpression>

static QString titleCaseImpl(const QString &text) {
    if (text.isEmpty())
        return QStringLiteral("App");
    QStringList parts = text.split(QRegularExpression(QStringLiteral("[-_.]+")), Qt::SkipEmptyParts);
    for (auto &p : parts) {
        if (!p.isEmpty())
            p[0] = p[0].toUpper();
    }
    return parts.join(QLatin1Char(' '));
}

bool WindowInfo::needsDeepIcon() const {
    const QString text = QStringList{
        className, initialClass, title, initialTitle
    }.join(QLatin1Char(' ')).toLower();
    return text.contains(QStringLiteral(".exe"))
        || text.contains(QStringLiteral("wine"))
        || text.contains(QStringLiteral("proton"))
        || text.contains(QStringLiteral("pressure-vessel"))
        || text.contains(QStringLiteral("steam_app_"));
}

WindowInfo WindowInfo::fromJson(const QJsonObject &obj) {
    WindowInfo info;
    const QJsonObject workspace = obj.value(QStringLiteral("workspace")).toObject();
    const bool hidden = obj.value(QStringLiteral("hidden")).toBool();
    
    QString addrStr = normalizeAddress(obj.value(QStringLiteral("address")).toString());
    info.windowId = WindowId{addrStr};
    info.pid = static_cast<qint64>(obj.value(QStringLiteral("pid")).toDouble());
    info.className = obj.value(QStringLiteral("class")).toString();
    info.initialClass = obj.value(QStringLiteral("initialClass")).toString();
    info.title = obj.value(QStringLiteral("title")).toString();
    info.initialTitle = obj.value(QStringLiteral("initialTitle")).toString();
    
    int wsVal = static_cast<int>(workspace.value(QStringLiteral("id")).toDouble());
    info.workspaceId = WorkspaceId{QString::number(wsVal)};
    info.workspaceName = workspace.value(QStringLiteral("name")).toString();
    info.focusHistoryId = static_cast<int>(obj.value(QStringLiteral("focusHistoryID")).toDouble(999999));
    info.displayName = displayNameFromMetadata(info.className, info.title);
    info.isActive = obj.value(QStringLiteral("active")).toBool();
    info.outputId = OutputId{obj.value(QStringLiteral("monitor")).toString()};
    
    info.isHidden = hidden;

    if (hidden || addrStr.isEmpty() || wsVal <= 0) {
        info.windowId = WindowId{};
    }
    return info;
}

QString WindowInfo::normalizeAddress(const QString &addr) {
    QString value = addr.trimmed().toLower();
    if (value.isEmpty())
        return {};
    if (value.startsWith(QStringLiteral("address:"))) {
        value = value.mid(8);
        if (value.isEmpty())
            return {};
    }
    if (value.startsWith(QStringLiteral("0x"))) {
        value = value.mid(2);
        if (value.isEmpty())
            return {};
    }
    if (value.isEmpty())
        return {};
    for (const QChar ch : value) {
        const QChar lower = ch.toLower();
        if (!ch.isDigit() && (lower < QLatin1Char('a') || lower > QLatin1Char('f')))
            return {};
    }
    return QStringLiteral("0x") + value;
}

QString WindowInfo::displayNameFromMetadata(const QString &className, const QString &title) {
    const QString cls = className.trimmed();
    const QString windowTitle = title.trimmed();
    if (cls.startsWith(QStringLiteral("steam_app_"), Qt::CaseInsensitive) && !windowTitle.isEmpty())
        return windowTitle;
    if (!cls.isEmpty() && cls != QStringLiteral("org.quickshell"))
        return titleCaseImpl(cls);
    return windowTitle.isEmpty() ? QStringLiteral("App") : windowTitle;
}
