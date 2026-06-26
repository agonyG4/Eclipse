#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

struct SearchResultItem {
    QString id;
    QString name;
    QString genericName;
    QString comment;
    QString icon;
    QString exec;
    QString desktopFileName;
    QString desktopFilePath;
    QString startupWmClass;
    bool terminal = false;
    QStringList keywords;
    QStringList categories;
    int score = 0;
    int usage = 0;
};

class SpotlightResultsModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        GenericNameRole,
        CommentRole,
        IconNameRole,
        EntryIdRole,
        ExecRole,
        DesktopFileNameRole,
        DesktopFilePathRole,
        StartupWmClassRole,
        TerminalRole,
        ScoreRole,
        UsageRole,
    };

    explicit SpotlightResultsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setResults(const QJsonArray &results);
    void clear();
    SearchResultItem resultAt(int row) const;
    int resultCount() const { return m_results.size(); }

private:
    QVector<SearchResultItem> m_results;
};
