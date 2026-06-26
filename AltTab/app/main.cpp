#include "app/AltTabApplication.hpp"
#include <QGuiApplication>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    AltTabApplication altTab(app);
    return altTab.run(argc, argv);
}
