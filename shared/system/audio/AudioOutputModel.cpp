#include "system/audio/AudioOutputModel.hpp"

#include <algorithm>

namespace Astrea::System {

AudioOutputModel::AudioOutputModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AudioOutputModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_outputs.size();
}

QVariant AudioOutputModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_outputs.size())
        return {};
    const AudioOutput &output = m_outputs.at(index.row());
    switch (role) {
    case NodeIdRole: return output.nodeId;
    case NameRole: return output.name;
    case DescriptionRole: return output.description;
    case NickRole: return output.nick;
    case MediaClassRole: return output.mediaClass;
    case DefaultRole: return output.isDefault;
    case VirtualRole: return output.isVirtual;
    case Qt::DisplayRole: return output.name;
    default: return {};
    }
}

QHash<int, QByteArray> AudioOutputModel::roleNames() const
{
    return {
        {NodeIdRole, "nodeId"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {NickRole, "nick"},
        {MediaClassRole, "mediaClass"},
        {DefaultRole, "isDefault"},
        {VirtualRole, "isVirtual"},
    };
}

void AudioOutputModel::replace(QVector<AudioOutput> outputs, quint32 defaultNodeId)
{
    for (AudioOutput &output : outputs)
        output.isDefault = output.nodeId == defaultNodeId;

    std::sort(outputs.begin(), outputs.end(), [](const AudioOutput &left,
                                                 const AudioOutput &right) {
        if (left.isDefault != right.isDefault)
            return left.isDefault > right.isDefault;
        const QString leftKey = left.name.isEmpty() ? left.description : left.name;
        const QString rightKey = right.name.isEmpty() ? right.description : right.name;
        const int nameCompare = QString::compare(leftKey, rightKey, Qt::CaseInsensitive);
        return nameCompare == 0 ? left.nodeId < right.nodeId : nameCompare < 0;
    });
    if (outputs == m_outputs)
        return;
    beginResetModel();
    m_outputs = std::move(outputs);
    endResetModel();
}

} // namespace Astrea::System
