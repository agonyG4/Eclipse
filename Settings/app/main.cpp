#include "app/SettingsApplication.hpp"

#include <QGuiApplication>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    SettingsApplication settingsApplication(app);
    return settingsApplication.run();
}
