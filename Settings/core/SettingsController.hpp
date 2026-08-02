#pragma once

#include "core/SettingsNavigationModel.hpp"

#include <QObject>
#include <QString>
#include <QUrl>

class SettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(SettingsNavigationModel *navigationModel READ navigationModel CONSTANT)
    Q_PROPERTY(QString selectedSectionId READ selectedSectionId NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedSectionTitle READ selectedSectionTitle NOTIFY selectionChanged)
    Q_PROPERTY(QString filterText READ filterText NOTIFY filterTextChanged)
    Q_PROPERTY(QString userName READ userName CONSTANT)
    Q_PROPERTY(QUrl avatarUrl READ avatarUrl CONSTANT)
    Q_PROPERTY(bool pagesAvailable READ pagesAvailable CONSTANT)

public:
    explicit SettingsController(QObject *parent = nullptr);

    SettingsNavigationModel *navigationModel();
    QString selectedSectionId() const;
    QString selectedSectionTitle() const;
    QString filterText() const;
    QString userName() const;
    QUrl avatarUrl() const;
    bool pagesAvailable() const;

    Q_INVOKABLE bool selectSection(const QString &id);
    Q_INVOKABLE void setFilterText(const QString &filterText);
    Q_INVOKABLE void clearFilter();

signals:
    void selectionChanged();
    void filterTextChanged();

private:
    static QString resolveUserName();
    static QUrl resolveAvatarUrl(const QString &userName);

    SettingsNavigationModel m_navigationModel;
    QString m_userName;
    QUrl m_avatarUrl;
};
