#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace Astrea::System {

struct AudioOutput {
    quint32 nodeId = 0;
    QString name;
    QString description;
    QString nick;
    QString mediaClass;
    bool isDefault = false;
    bool isVirtual = false;

    friend bool operator==(const AudioOutput &, const AudioOutput &) = default;
};

class AudioOutputModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        NodeIdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        NickRole,
        MediaClassRole,
        DefaultRole,
        VirtualRole,
    };
    Q_ENUM(Role)

    explicit AudioOutputModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void replace(QVector<AudioOutput> outputs, quint32 defaultNodeId);

private:
    QVector<AudioOutput> m_outputs;
};

} // namespace Astrea::System

Q_DECLARE_METATYPE(Astrea::System::AudioOutput)
