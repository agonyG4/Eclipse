#include <AstreaSDK/Core.hpp>
#include <AstreaSDK/FileIconProvider.hpp>

#include "icons/AstreaIconProvider.hpp"

#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlImageProviderBase>
#include <QQuickImageProvider>

#include <QDebug>

namespace {

constexpr auto kIconProviderId = "astrea-icon";
constexpr auto kProviderContextName = "AstreaIconProvider";
constexpr auto kInitializationProperty = "_astrea_sdk_core_initialized";

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

} // namespace

bool AstreaSDK::Core::initializeQmlEngine(QQmlEngine &engine, QString *error)
{
    if (error)
        error->clear();

    QQmlImageProviderBase *registered = engine.imageProvider(
        QLatin1String(kIconProviderId));
    if (registered) {
        auto *iconProvider = dynamic_cast<AstreaIconProvider *>(registered);
        if (!iconProvider) {
            setError(error, QStringLiteral(
                "The QML engine already has a non-Astrea provider registered as '%1'.")
                           .arg(QLatin1String(kIconProviderId)));
            return false;
        }
        engine.rootContext()->setContextProperty(QLatin1String(kProviderContextName),
                                                 iconProvider);
        engine.setProperty(kInitializationProperty, true);
        return true;
    }

    if (engine.property(kInitializationProperty).toBool()) {
        setError(error, QStringLiteral(
            "The Astrea SDK icon provider was removed after engine initialization."));
        return false;
    }

    auto *iconProvider = new AstreaIconProvider;
    engine.addImageProvider(QLatin1String(kIconProviderId), iconProvider);
    engine.rootContext()->setContextProperty(QLatin1String(kProviderContextName),
                                             iconProvider);
    engine.setProperty(kInitializationProperty, true);
    return true;
}

bool AstreaSDK::Core::initializeFileIcons(QQmlEngine &engine,
    IconThemeService &service, QString *error)
{
    if (error)
        error->clear();
    if (engine.thread() != service.thread()) {
        setError(error, QStringLiteral("File icons and the QML engine must share a thread."));
        return false;
    }
    auto *registered = engine.imageProvider(QStringLiteral("astrea-icons"));
    if (registered) {
        auto *provider = dynamic_cast<FileIconProvider *>(registered);
        if (!provider || provider->service() != &service) {
            setError(error, QStringLiteral("The astrea-icons provider is already registered with another service."));
            return false;
        }
    } else {
        engine.addImageProvider(QStringLiteral("astrea-icons"), new FileIconProvider(&service));
    }
    engine.rootContext()->setContextProperty(QStringLiteral("AstreaFileIcons"), &service);
    return true;
}
