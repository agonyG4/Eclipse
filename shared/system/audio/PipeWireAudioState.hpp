#pragma once

#include "system/audio/AudioOutputModel.hpp"

#include <QVector>

#include <map>

namespace Astrea::System {

struct PipeWireNodeAudioState {
    quint32 nodeId = 0;
    AudioOutput output;
    float volume = 1.0f;
    QVector<float> channelVolumes;
    bool muted = false;
    bool volumeKnown = false;
    bool muteKnown = false;
};

class PipeWireAudioState final {
public:
    void reset(quint64 generation);
    quint64 generation() const { return m_generation; }
    void upsertNode(PipeWireNodeAudioState node);
    void updateNodeMetadata(quint32 nodeId, const QString &name,
                            const QString &description, const QString &nick);
    void removeNode(quint32 nodeId);
    void updateNodeProps(quint32 nodeId, float volume, bool volumeKnown,
                         bool muted, bool muteKnown, QVector<float> channelVolumes);
    void setMetadataDefault(QString nodeName, quint32 nodeId = 0);
    void clearMetadataDefault();
    quint32 defaultNodeId() const;
    QVector<AudioOutput> outputs() const;
    bool defaultVolume(float *volume, bool *muted) const;
    bool hasDefaultMetadata() const { return m_metadataPresent; }

private:
    quint64 m_generation = 0;
    bool m_metadataPresent = false;
    QString m_defaultName;
    quint32 m_defaultId = 0;
    std::map<quint32, PipeWireNodeAudioState> m_nodes;
};

} // namespace Astrea::System
