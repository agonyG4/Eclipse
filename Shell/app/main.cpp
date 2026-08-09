#include "app/AstreaShellApplication.hpp"

#include <QGuiApplication>

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    AstreaShellApplication shell(application);
    return shell.run();
}
