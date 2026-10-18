#pragma once

#include <QJsonArray>
#include <QObject>
#include <QStringList>

class SpotlightCatalogSource : public QObject {
    Q_OBJECT

public:
    explicit SpotlightCatalogSource(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~SpotlightCatalogSource() override = default;

    virtual QJsonArray snapshotJson() const = 0;
    virtual QStringList watchedDirectories() const = 0;

signals:
    void updated();
};
