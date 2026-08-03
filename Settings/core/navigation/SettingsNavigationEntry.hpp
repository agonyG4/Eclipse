#pragma once

#include <QUrl>
#include <QString>

struct SettingsNavigationEntry {
    enum class Kind {
        Page,
        Group,
        Spacer,
    };

    QString id;
    QString label;
    QString labelKey;
    QString subtitle;
    QString sym;
    QString iconSource;
    QString iconKey;
    QUrl pageSource;
    Kind kind = Kind::Page;
    bool enabled = true;
};
