#include "app/AstreaShellApplication.hpp"
#include "platform/wayland/LayerShellHelper.hpp"

#include <QDebug>
#include <QGuiApplication>

int main(int argc, char **argv)
{
    QString error;
    if (!AstreaLayerShellHelper::prepare(&error)) {
        qCritical("Astrea shell Layer Shell preparation failed: %s", qPrintable(error));
        return 1;
    }
    QGuiApplication application(argc, argv);
    AstreaShellApplication shell(application);
    return shell.run();
}
