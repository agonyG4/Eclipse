#include "platform/unity/UnityLauncherEntryService.hpp"

#include "core/DockAppModel.hpp"

#include <QDBusError>
#include <QSet>
#include <QUrl>

#include <limits>

namespace {
constexpr auto kServiceName = "com.canonical.Unity";
constexpr auto kUnityObjectPath = "/Unity";
constexpr auto kInterfaceName = "com.canonical.Unity.LauncherEntry";
}

UnityLauncherEntryService::UnityLauncherEntryService(DockAppModel *model,
                                                     QDBusConnection connection,
                                                     QObject *parent)
    : QObject(parent), m_model(model), m_connection(std::move(connection))
{
}

UnityLauncherEntryService::~UnityLauncherEntryService()
{
    stop();
}

bool UnityLauncherEntryService::start()
{
    if (m_started)
        return true;
    if (!m_model || !m_connection.isConnected())
        return false;
    if (!m_connection.connect(QString(), QString(), QString::fromLatin1(kInterfaceName),
                              QStringLiteral("Update"), this,
                              SLOT(onUpdate(QString,QVariantMap,QDBusMessage))))
        return false;
    if (!m_connection.connect(QStringLiteral("org.freedesktop.DBus"),
                              QStringLiteral("/org/freedesktop/DBus"),
                              QStringLiteral("org.freedesktop.DBus"),
                              QStringLiteral("NameOwnerChanged"), this,
                              SLOT(onNameOwnerChanged(QString,QString,QString)))) {
        m_connection.disconnect(QString(), QString(), QString::fromLatin1(kInterfaceName),
                                QStringLiteral("Update"), this,
                                SLOT(onUpdate(QString,QVariantMap,QDBusMessage)));
        return false;
    }
    m_started = true;
    // Subscribe before advertising the host so clients that broadcast their
    // current counter immediately cannot race listener registration.
    m_ownsService = m_connection.registerService(QString::fromLatin1(kServiceName));
    if (m_ownsService) {
        m_registeredUnityObject = m_connection.registerObject(
            QString::fromLatin1(kUnityObjectPath), this, QDBusConnection::ExportScriptableSlots);
        if (!m_registeredUnityObject) {
            m_connection.unregisterService(QString::fromLatin1(kServiceName));
            m_ownsService = false;
        }
    }
    return true;
}

void UnityLauncherEntryService::stop()
{
    if (!m_started)
        return;
    m_connection.disconnect(QStringLiteral("org.freedesktop.DBus"),
                            QStringLiteral("/org/freedesktop/DBus"),
                            QStringLiteral("org.freedesktop.DBus"),
                            QStringLiteral("NameOwnerChanged"), this,
                            SLOT(onNameOwnerChanged(QString,QString,QString)));
    m_connection.disconnect(QString(), QString(), QString::fromLatin1(kInterfaceName),
                            QStringLiteral("Update"), this,
                            SLOT(onUpdate(QString,QVariantMap,QDBusMessage)));
    if (m_registeredUnityObject)
        m_connection.unregisterObject(QString::fromLatin1(kUnityObjectPath));
    if (m_ownsService)
        m_connection.unregisterService(QString::fromLatin1(kServiceName));
    m_registeredUnityObject = false;
    m_ownsService = false;
    m_started = false;
    QSet<QString> desktopSet;
    for (auto sender = m_senderStates.cbegin(); sender != m_senderStates.cend(); ++sender) {
        for (auto app = sender->cbegin(); app != sender->cend(); ++app) {
            desktopSet.insert(app.key());
            emit senderUnreadChanged(sender.key(), app.key(), 0, false);
        }
    }
    m_senderStates.clear();
    for (const QString &desktopFileName : std::as_const(desktopSet))
        m_model->applyUnreadCount(desktopFileName, 0, false);
}

void UnityLauncherEntryService::onUpdate(const QString &applicationUri,
                                         const QVariantMap &properties,
                                         const QDBusMessage &dbusMessage)
{
    if (!m_started)
        return;
    const QString desktopFileName = desktopFileNameFromUri(applicationUri);
    const QString sender = dbusMessage.service();
    if (desktopFileName.isEmpty() || !sender.startsWith(QLatin1Char(':')))
        return;

    bool hasCountUpdate = false;
    int updatedCount = 0;
    const auto count = properties.constFind(QStringLiteral("count"));
    if (count != properties.cend()) {
        hasCountUpdate = integerCount(count.value(), &updatedCount);
    }
    const auto visible = properties.constFind(QStringLiteral("count-visible"));
    const bool hasVisibilityUpdate = visible != properties.cend()
        && visible->metaType() == QMetaType::fromType<bool>();
    if (!hasCountUpdate && !hasVisibilityUpdate)
        return;

    SenderState &state = m_senderStates[sender][desktopFileName];
    if (hasCountUpdate) {
        state.hasCount = true;
        state.count = updatedCount;
        state.countOrder = ++m_updateOrder;
    }
    if (hasVisibilityUpdate) {
        state.hasVisibility = true;
        state.visible = visible->toBool();
        state.visibilityOrder = ++m_updateOrder;
    }
    refreshDesktop(desktopFileName);
    emit senderUnreadChanged(sender, desktopFileName, state.hasCount ? state.count : 0,
                             state.hasVisibility && state.visible);
}

void UnityLauncherEntryService::onNameOwnerChanged(const QString &name,
                                                    const QString &oldOwner,
                                                    const QString &newOwner)
{
    if (name == oldOwner && name.startsWith(QLatin1Char(':')) && newOwner.isEmpty())
        clearSender(oldOwner);
}

QString UnityLauncherEntryService::desktopFileNameFromUri(const QString &applicationUri)
{
    const QString prefix = QStringLiteral("application://");
    if (!applicationUri.startsWith(prefix))
        return {};
    const QUrl url(applicationUri, QUrl::StrictMode);
    if (!url.isValid() || url.scheme() != QLatin1String("application") || !url.path().isEmpty()
        || !url.query().isEmpty() || !url.fragment().isEmpty() || !url.userInfo().isEmpty()
        || url.port(-1) != -1)
        return {};
    const QByteArray encoded = applicationUri.mid(prefix.size()).toUtf8();
    const QString desktopFileName = QUrl::fromPercentEncoding(encoded);
    if (desktopFileName.isEmpty() || desktopFileName.contains(QLatin1Char('/'))
        || desktopFileName.contains(QLatin1Char('\\'))
        || desktopFileName == QStringLiteral(".")
        || desktopFileName == QStringLiteral("..")
        || !desktopFileName.endsWith(QStringLiteral(".desktop"), Qt::CaseSensitive))
        return {};
    return desktopFileName;
}

bool UnityLauncherEntryService::integerCount(const QVariant &value, int *count)
{
    const QMetaType type = value.metaType();
    if (type == QMetaType::fromType<int>()) {
        const int parsed = value.toInt();
        if (parsed < 0)
            return false;
        *count = parsed;
        return true;
    }
    if (type == QMetaType::fromType<uint>()) {
        const uint parsed = value.toUInt();
        if (parsed > uint(std::numeric_limits<int>::max()))
            return false;
        *count = int(parsed);
        return true;
    }
    if (type == QMetaType::fromType<qlonglong>()) {
        const qlonglong parsed = value.toLongLong();
        if (parsed < 0 || parsed > std::numeric_limits<int>::max())
            return false;
        *count = int(parsed);
        return true;
    }
    if (type == QMetaType::fromType<qulonglong>()) {
        const qulonglong parsed = value.toULongLong();
        if (parsed > qulonglong(std::numeric_limits<int>::max()))
            return false;
        *count = int(parsed);
        return true;
    }
    return false;
}

void UnityLauncherEntryService::clearSender(const QString &sender)
{
    const auto found = m_senderStates.find(sender);
    if (found == m_senderStates.end())
        return;
    const QStringList desktops = found->keys();
    m_senderStates.erase(found);
    for (const QString &desktopFileName : desktops) {
        refreshDesktop(desktopFileName);
        emit senderUnreadChanged(sender, desktopFileName, 0, false);
    }
}

void UnityLauncherEntryService::refreshDesktop(const QString &desktopFileName)
{
    bool hasCount = false;
    int count = 0;
    quint64 countOrder = 0;
    bool hasVisibility = false;
    bool visible = false;
    quint64 visibilityOrder = 0;
    for (auto sender = m_senderStates.cbegin(); sender != m_senderStates.cend(); ++sender) {
        const auto state = sender->constFind(desktopFileName);
        if (state == sender->cend())
            continue;
        if (state->hasCount && state->countOrder >= countOrder) {
            hasCount = true;
            count = state->count;
            countOrder = state->countOrder;
        }
        if (state->hasVisibility && state->visibilityOrder >= visibilityOrder) {
            hasVisibility = true;
            visible = state->visible;
            visibilityOrder = state->visibilityOrder;
        }
    }
    m_model->applyUnreadCount(desktopFileName, hasCount ? count : 0,
                              hasVisibility && visible);
}
