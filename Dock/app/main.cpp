#include "app/DockApplication.hpp"

#include <QGuiApplication>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    DockApplication application(app);
    return application.run();
}
