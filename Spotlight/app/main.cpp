#include "app/SpotlightApplication.hpp"

#include <QGuiApplication>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    SpotlightApplication spotlight(app);
    return spotlight.run(argc, argv);
}
