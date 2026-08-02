#include "app/SettingsApplication.hpp"

#include "core/SettingsController.hpp"
#include "icons/AstreaIconProvider.hpp"
#include "icons/AstreaIconTheme.hpp"

#include <QGuiApplication>
#include <QIcon>
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

    m_controller = std::make_unique<SettingsController>();
    if (!initializeQml())
        return 1;

    return m_app.exec();
}

bool SettingsApplication::initializeQml()
{
    m_engine = std::make_unique<QQmlApplicationEngine>();
    QObject::connect(m_engine.get(), &QQmlApplicationEngine::warnings, this,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            qWarning("Settings QML: %s", qPrintable(warning.toString()));
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

    m_engine->loadFromModule(QStringLiteral("Astrea.Settings"), QStringLiteral("Main"));
    if (m_engine->rootObjects().size() != 1) {
        qCritical("Settings expected exactly one QML root object, received %lld",
                  static_cast<long long>(m_engine->rootObjects().size()));
        return false;
    }

    return true;
}
