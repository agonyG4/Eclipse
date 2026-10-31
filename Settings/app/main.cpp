#include "app/SettingsApplication.hpp"
#include "app/SettingsFontPolicy.hpp"

#include <QGuiApplication>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    applySettingsFontPolicy(app);
    SettingsApplication settingsApplication(app);
    return settingsApplication.run();
}
