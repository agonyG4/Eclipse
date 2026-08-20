#include "system/audio/PipeWireAudioState.hpp"

#include <algorithm>

namespace Astrea::System {

void PipeWireAudioState::reset(quint64 generation)
{
    m_generation = generation;
    m_metadataPresent = false;
    m_defaultName.clear();
    m_defaultId = 0;
    m_nodes.clear();
}

void PipeWireAudioState::upsertNode(PipeWireNodeAudioState node)
{
    m_nodes[node.nodeId] = std::move(node);
}

void PipeWireAudioState::updateNodeMetadata(quint32 nodeId, const QString &name,
                                            const QString &description, const QString &nick)
{
    auto node = m_nodes.find(nodeId);
    if (node == m_nodes.end())
        return;
    if (!name.isEmpty())
        node->second.output.name = name;
    if (!description.isEmpty())
        node->second.output.description = description;
    if (!nick.isEmpty())
        node->second.output.nick = nick;
}

void PipeWireAudioState::removeNode(quint32 nodeId)
{
    m_nodes.erase(nodeId);
}

void PipeWireAudioState::updateNodeProps(quint32 nodeId, float volume, bool volumeKnown,
                                         bool muted, bool muteKnown,
                                         QVector<float> channelVolumes)
{
    auto node = m_nodes.find(nodeId);
    if (node == m_nodes.end())
        return;
    node->second.volume = volume;
    node->second.volumeKnown = volumeKnown;
    node->second.muted = muted;
    node->second.muteKnown = muteKnown;
    node->second.channelVolumes = std::move(channelVolumes);
}

void PipeWireAudioState::setMetadataDefault(QString nodeName, quint32 nodeId)
{
    m_metadataPresent = true;
    m_defaultName = std::move(nodeName);
    m_defaultId = nodeId;
}

void PipeWireAudioState::clearMetadataDefault()
{
    m_metadataPresent = false;
    m_defaultName.clear();
    m_defaultId = 0;
}

quint32 PipeWireAudioState::defaultNodeId() const
{
    if (!m_metadataPresent)
        return 0;
    if (m_defaultId != 0 && m_nodes.contains(m_defaultId))
        return m_defaultId;
    if (m_defaultName.isEmpty())
        return 0;
    for (const auto &[id, node] : m_nodes) {
        if (node.output.name == m_defaultName)
            return id;
    }
    return 0;
}

QVector<AudioOutput> PipeWireAudioState::outputs() const
{
    QVector<AudioOutput> result;
    result.reserve(static_cast<qsizetype>(m_nodes.size()));
    const quint32 defaultId = defaultNodeId();
    for (const auto &[id, node] : m_nodes) {
        AudioOutput output = node.output;
        output.isDefault = id == defaultId;
        result.append(std::move(output));
    }
    return result;
}

bool PipeWireAudioState::defaultVolume(float *volume, bool *muted) const
{
    const auto node = m_nodes.find(defaultNodeId());
    if (node == m_nodes.end())
        return false;
    const PipeWireNodeAudioState &state = node->second;
    if (!state.volumeKnown && state.channelVolumes.isEmpty() && !state.muteKnown)
        return false;
    float aggregate = state.volume;
    if (!state.channelVolumes.isEmpty()) {
        double total = 0.0;
        for (const float channel : state.channelVolumes)
            total += channel;
        aggregate = static_cast<float>(total / state.channelVolumes.size());
    }
    if (volume)
        *volume = std::max(0.0f, aggregate);
    if (muted)
        *muted = state.muted;
    return true;
}

} // namespace Astrea::System
