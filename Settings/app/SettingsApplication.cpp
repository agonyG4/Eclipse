#include "app/SettingsApplication.hpp"

#include "core/SettingsController.hpp"
#include "core/navigation/SettingsNavigationCatalog.hpp"
#include "core/navigation/SettingsNavigationModel.hpp"
#include "icons/AstreaIconProvider.hpp"
#include "icons/AstreaIconTheme.hpp"
#include "platform/linux/AdminGroupDetector.hpp"
#include "services/assets/SettingsIconResolver.hpp"
#include "services/i18n/SettingsTranslationController.hpp"
#include "services/profile/SettingsUserProfileProvider.hpp"
#include "theme/ThemeController.hpp"

#include <QGuiApplication>
#include <QIcon>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>

SettingsApplication::SettingsApplication(QGuiApplication &app)
    : QObject(&app)
    , m_app(app)
{
}

SettingsApplication::~SettingsApplication() = default;

int SettingsApplication::run()
{
    m_app.setApplicationName(QStringLiteral("Astrea Settings"));
    m_app.setApplicationDisplayName(QStringLiteral("Astrea Settings"));
    m_app.setOrganizationName(QStringLiteral("AstreaOS"));
    m_app.setOrganizationDomain(QStringLiteral("astreaos.local"));
    m_app.setDesktopFileName(QStringLiteral("astrea-settings"));
    m_app.setQuitOnLastWindowClosed(true);

    AstreaIconTheme::apply();
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    AdminGroupDetector adminGroupDetector;
    SettingsUserProfileProvider profileProvider(adminGroupDetector);
    const SettingsUserProfile userProfile = profileProvider.currentProfile();

    SettingsNavigationCatalog navigationCatalog;
    auto navigationModel = std::make_unique<SettingsNavigationModel>(navigationCatalog);
    SettingsIconResolver iconResolver;
    m_controller = std::make_unique<SettingsController>(std::move(navigationModel), userProfile,
                                                         iconResolver, this);
    m_translationController = std::make_unique<SettingsTranslationController>();
    m_themeController = std::make_unique<ThemeController>();
    if (!initializeQml())
        return 1;

    return m_app.exec();
}

bool SettingsApplication::initializeQml()
{
    m_engine = std::make_unique<QQmlApplicationEngine>();
    QObject::connect(m_engine.get(), &QQmlApplicationEngine::warnings, this,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings) {
            qWarning("Settings QML: %s", qPrintable(warning.toString()));
        }
    });
    QObject::connect(m_engine.get(), &QQmlApplicationEngine::objectCreationFailed, this,
                     [](const QUrl &url) {
         qCritical("Settings QML object creation failed: %s", qPrintable(url.toString()));
    });

    m_iconProvider = new AstreaIconProvider;
    m_engine->addImportPath(QStringLiteral("qrc:/"));
    m_engine->addImageProvider(QStringLiteral("astrea-icon"), m_iconProvider);
    m_engine->rootContext()->setContextProperty(QStringLiteral("AstreaIconProvider"),
                                                m_iconProvider);
    m_engine->rootContext()->setContextProperty(QStringLiteral("SettingsController"),
                                                m_controller.get());
    m_engine->rootContext()->setContextProperty(QStringLiteral("I18n"),
                                                m_translationController.get());
    m_engine->rootContext()->setContextProperty(QStringLiteral("ThemeController"),
                                                m_themeController.get());

    m_engine->load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));
    if (m_engine->rootObjects().size() != 1) {
        qCritical("Settings expected exactly one QML root object, received %lld",
                  static_cast<long long>(m_engine->rootObjects().size()));
        return false;
    }

    return true;
}
