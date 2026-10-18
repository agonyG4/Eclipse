#include "system/audio/PipeWireAudioBackend.hpp"

#include "system/audio/PipeWireAudioState.hpp"

#include <pipewire/core.h>
#include <pipewire/extensions/metadata.h>
#include <pipewire/keys.h>
#include <pipewire/node.h>
#include <pipewire/pipewire.h>
#include <pipewire/proxy.h>
#include <pipewire/thread-loop.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <spa/param/param.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>
#include <spa/pod/iter.h>
#include <spa/utils/dict.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <map>
#include <mutex>
#include <QVector>

namespace Astrea::System {

struct PipeWireAudioBackend::Impl {
    struct NodeBinding {
        Impl *owner = nullptr;
        quint32 id = 0;
        pw_node *node = nullptr;
        spa_hook listener{};
        pw_node_events events{};
        float volume = 1.0f;
        bool muted = false;
        QVector<float> channelVolumes;
        bool volumeKnown = false;
        bool muteKnown = false;
    };

    pw_thread_loop *loop = nullptr;
    pw_context *context = nullptr;
    pw_core *core = nullptr;
    pw_registry *registry = nullptr;
    pw_metadata *metadata = nullptr;
    quint32 metadataId = 0;
    bool threadStarted = false;
    bool coreListenerAdded = false;
    bool registryListenerAdded = false;
    spa_hook registryListener{};
    spa_hook coreListener{};
    spa_hook metadataListener{};
    pw_core_events coreEvents{};
    pw_registry_events registryEvents{};
    pw_metadata_events metadataEvents{};
    Callbacks callbacks;
    std::map<quint32, AudioOutput> outputs;
    std::map<quint32, AudioOutput> inputs;
    QString defaultInputName;
    quint32 defaultInputId = 0;
    std::map<quint32, pw_node *> nodes;
    std::map<quint32, std::unique_ptr<NodeBinding>> nodeBindings;
    PipeWireAudioState state;
    quint64 attemptGeneration = 0;
    QString defaultName;
    quint32 metadataDefaultId = 0;

    void releaseMetadataProxy()
    {
        if (metadata) {
            spa_hook_remove(&metadataListener);
            pw_proxy_destroy(reinterpret_cast<pw_proxy *>(metadata));
            metadata = nullptr;
        }
        metadataId = 0;
    }

    void releaseNodeProxy(quint32 id)
    {
        const auto bindingIt = nodeBindings.find(id);
        const auto nodeIt = nodes.find(id);
        pw_node *node = nodeIt != nodes.end() && nodeIt->second
            ? nodeIt->second
            : bindingIt != nodeBindings.end() && bindingIt->second
                ? bindingIt->second->node
                : nullptr;

        if (bindingIt != nodeBindings.end() && bindingIt->second)
            spa_hook_remove(&bindingIt->second->listener);
        if (node)
            pw_proxy_destroy(reinterpret_cast<pw_proxy *>(node));
        if (bindingIt != nodeBindings.end())
            nodeBindings.erase(bindingIt);
        if (nodeIt != nodes.end())
            nodes.erase(nodeIt);
    }

    void releaseAllNodeProxies()
    {
        while (!nodes.empty())
            releaseNodeProxy(nodes.begin()->first);
        while (!nodeBindings.empty())
            releaseNodeProxy(nodeBindings.begin()->first);
    }

    quint32 defaultInputNodeId() const
    {
        if (defaultInputId && inputs.contains(defaultInputId)) return defaultInputId;
        for (const auto &[id, input] : inputs)
            if (!defaultInputName.isEmpty() && input.name == defaultInputName) return id;
        return 0;
    }

    void publishInputs()
    {
        if (!callbacks.inputsChanged) return;
        const auto chosen = defaultInputNodeId();
        QVector<AudioOutput> values;
        for (const auto &[id, input] : inputs) values.append(input);
        callbacks.inputsChanged(std::move(values), chosen);
    }

    void publishInputVolume()
    {
        if (!callbacks.inputVolumeChanged) return;
        const auto it = nodeBindings.find(defaultInputNodeId());
        if (it == nodeBindings.end() || !it->second) return;
        const auto &node = *it->second;
        if (!node.muteKnown || (!node.volumeKnown && node.channelVolumes.isEmpty())) return;
        double gain = node.volume;
        if (!node.channelVolumes.isEmpty()) {
            gain = 0;
            for (float v : node.channelVolumes) gain += v;
            gain /= node.channelVolumes.size();
        }
        callbacks.inputVolumeChanged(gain, node.muted);
    }

    quint32 defaultNodeId() const
    {
        return state.defaultNodeId();
    }

    static void registryGlobal(void *data, uint32_t id, uint32_t, const char *type,
                               uint32_t, const spa_dict *props)
    {
        auto *impl = static_cast<Impl *>(data);
        if (!type)
            return;
        if (std::strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0) {
            const char *metadataName = spa_dict_lookup(props, PW_KEY_METADATA_NAME);
            if (!metadataName || std::strcmp(metadataName, "default") != 0
                || impl->metadata)
                return;
            impl->metadata = static_cast<pw_metadata *>(
                pw_registry_bind(impl->registry, id, PW_TYPE_INTERFACE_Metadata,
                                 PW_VERSION_METADATA, 0));
            if (impl->metadata) {
                impl->metadataId = id;
                impl->metadataEvents.version = PW_VERSION_METADATA_EVENTS;
                impl->metadataEvents.property = &Impl::metadataProperty;
                pw_metadata_add_listener(impl->metadata, &impl->metadataListener,
                                         &impl->metadataEvents, impl);
            }
            return;
        }
        if (std::strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
            return;
        const char *mediaClass = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
        const bool isInput = mediaClass && std::strcmp(mediaClass, "Audio/Source") == 0;
        if (!mediaClass || (!isInput && std::strcmp(mediaClass, "Audio/Sink") != 0))
            return;
        const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
        const char *description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
        const char *nick = spa_dict_lookup(props, PW_KEY_NODE_NICK);
        const char *virtualValue = spa_dict_lookup(props, "node.virtual");
        AudioOutput output;
        output.nodeId = id;
        output.name = QString::fromUtf8(name ? name : "");
        output.description = QString::fromUtf8(description ? description : "");
        output.nick = QString::fromUtf8(nick ? nick : "");
        output.mediaClass = QString::fromUtf8(mediaClass);
        output.isVirtual = virtualValue && QString::fromUtf8(virtualValue) == QStringLiteral("true");
        if (isInput) impl->inputs[id] = output;
        else impl->outputs[id] = output;
        PipeWireNodeAudioState nodeState;
        nodeState.nodeId = id;
        nodeState.output = output;
        if (!isInput) impl->state.upsertNode(std::move(nodeState));
        impl->nodes[id] = static_cast<pw_node *>(
            pw_registry_bind(impl->registry, id, PW_TYPE_INTERFACE_Node,
                             PW_VERSION_NODE, 0));
        if (impl->nodes[id]) {
            auto binding = std::make_unique<NodeBinding>();
            binding->owner = impl;
            binding->id = id;
            binding->node = impl->nodes[id];
            binding->events.version = PW_VERSION_NODE_EVENTS;
            binding->events.info = &Impl::nodeInfo;
            binding->events.param = &Impl::nodeParam;
            pw_node_add_listener(binding->node, &binding->listener, &binding->events,
                                 binding.get());
            uint32_t params[] = {SPA_PARAM_Props};
            pw_node_subscribe_params(binding->node, params, 1);
            pw_node_enum_params(binding->node, 0, SPA_PARAM_Props, 0, 1, nullptr);
            impl->nodeBindings[id] = std::move(binding);
        }
        if (isInput) { impl->publishInputs(); impl->publishInputVolume(); }
        else { impl->publishOutputs(); impl->publishDefaultVolume(); }
    }

    static void coreError(void *data, uint32_t, int, int, const char *message)
    {
        auto *impl = static_cast<Impl *>(data);
        if (impl->callbacks.defaultStateChanged)
            impl->callbacks.defaultStateChanged(false, false,
                                                 QString::fromUtf8(message ? message : ""));
    }

    static void registryGlobalRemove(void *data, uint32_t id)
    {
        auto *impl = static_cast<Impl *>(data);
        if (impl->metadata && id == impl->metadataId) {
            impl->releaseMetadataProxy();
            impl->state.clearMetadataDefault();
            impl->publishOutputs();
            return;
        }
        impl->releaseNodeProxy(id);
        impl->outputs.erase(id);
        impl->inputs.erase(id);
        impl->state.removeNode(id);
        impl->publishOutputs();
        impl->publishInputs();
    }

    static int metadataProperty(void *data, uint32_t, const char *key,
                                const char *, const char *value)
    {
        auto *impl = static_cast<Impl *>(data);
        if (!key) return 0;
        if (std::strcmp(key, "default.audio.source") == 0) {
            impl->defaultInputId = 0;
            impl->defaultInputName.clear();
            if (value) {
                const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(value));
                if (doc.isObject()) {
                    impl->defaultInputId = doc.object().value(QStringLiteral("id")).toInt();
                    impl->defaultInputName = doc.object().value(QStringLiteral("name")).toString();
                } else impl->defaultInputName = QString::fromUtf8(value);
            }
            impl->publishInputs();
            impl->publishInputVolume();
            return 0;
        }
        if (std::strcmp(key, "default.audio.sink") != 0) return 0;
        impl->metadataDefaultId = 0;
        impl->defaultName.clear();
        if (value) {
            const QByteArray json = QByteArray(value);
            const QJsonDocument document = QJsonDocument::fromJson(json);
            if (document.isObject()) {
                const QJsonObject object = document.object();
                impl->metadataDefaultId = object.value(QStringLiteral("id")).toInt();
                impl->defaultName = object.value(QStringLiteral("name")).toString();
            } else {
                impl->defaultName = QString::fromUtf8(value);
            }
            impl->state.setMetadataDefault(impl->defaultName, impl->metadataDefaultId);
        } else {
            impl->state.clearMetadataDefault();
        }
        impl->publishOutputs();
        impl->publishDefaultVolume();
        return 0;
    }

    static void nodeInfo(void *data, const pw_node_info *info)
    {
        if (!info)
            return;
        auto *binding = static_cast<NodeBinding *>(data);
        auto it = binding->owner->outputs.find(binding->id);
        const bool isInput = binding->owner->inputs.contains(binding->id);
        if (isInput) it = binding->owner->outputs.end();
        if (!isInput && it == binding->owner->outputs.end()) return;
        AudioOutput &output = isInput ? binding->owner->inputs.at(binding->id) : it->second;
        const char *name = spa_dict_lookup(info->props, PW_KEY_NODE_NAME);
        const char *description = spa_dict_lookup(info->props, PW_KEY_NODE_DESCRIPTION);
        const char *nick = spa_dict_lookup(info->props, PW_KEY_NODE_NICK);
        if (name)
            output.name = QString::fromUtf8(name);
        if (description)
            output.description = QString::fromUtf8(description);
        if (nick)
            output.nick = QString::fromUtf8(nick);
        if (isInput) binding->owner->publishInputs();
        else { binding->owner->state.updateNodeMetadata(binding->id, output.name, output.description, output.nick);
               binding->owner->publishOutputs(); }
    }

    static void nodeParam(void *data, int, uint32_t id, uint32_t, uint32_t,
                          const spa_pod *param)
    {
        if (id != SPA_PARAM_Props || !param || !spa_pod_is_object(param))
            return;
        auto *binding = static_cast<NodeBinding *>(data);
        const auto *object = reinterpret_cast<const spa_pod_object *>(param);
        const spa_pod_prop *volumeProperty =
            spa_pod_object_find_prop(object, nullptr, SPA_PROP_volume);
        const spa_pod_prop *muteProperty =
            spa_pod_object_find_prop(object, nullptr, SPA_PROP_mute);
        PipeWireNodePropsPatch patch;
        if (volumeProperty) {
            float volume = binding->volume;
            if (spa_pod_get_float(&volumeProperty->value, &volume) >= 0) {
                binding->volume = volume;
                binding->volumeKnown = true;
                patch.volume = volume;
            }
        }
        if (muteProperty) {
            bool muted = binding->muted;
            if (spa_pod_get_bool(&muteProperty->value, &muted) >= 0) {
                binding->muted = muted;
                binding->muteKnown = true;
                patch.muted = muted;
            }
        }
        const spa_pod_prop *channelProperty =
            spa_pod_object_find_prop(object, nullptr, SPA_PROP_channelVolumes);
        if (channelProperty) {
            uint32_t count = 0;
            const auto *values = static_cast<const float *>(
                spa_pod_get_array(&channelProperty->value, &count));
            QVector<float> channelVolumes;
            if (values && count > 0) {
                channelVolumes.reserve(static_cast<qsizetype>(count));
                for (uint32_t index = 0; index < count; ++index)
                    channelVolumes.append(values[index]);
            }
            binding->channelVolumes = channelVolumes;
            patch.channelVolumes = std::move(channelVolumes);
        }
        binding->owner->state.applyNodePropsPatch(binding->id, patch);
        if (binding->id == binding->owner->defaultNodeId())
            binding->owner->publishDefaultVolume();
        if (binding->id == binding->owner->defaultInputNodeId())
            binding->owner->publishInputVolume();
    }

    void publishOutputs()
    {
        if (!callbacks.outputsChanged)
            return;
        QVector<AudioOutput> values;
        values = state.outputs();
        const quint32 defaultNodeId = this->defaultNodeId();
        callbacks.outputsChanged(std::move(values), defaultNodeId);
    }

    void publishDefaultVolume()
    {
        if (!callbacks.volumeChanged)
            return;
        float volume = 0.0f;
        bool muted = false;
        if (!state.defaultVolume(&volume, &muted))
            return;
        callbacks.volumeChanged(volume, muted);
    }

    bool setProps(quint32 nodeId, const spa_pod *pod)
    {
        const auto it = nodes.find(nodeId);
        return it != nodes.end() && it->second
            && pw_node_set_param(it->second, SPA_PARAM_Props, 0, pod) >= 0;
    }
};

PipeWireAudioBackend::PipeWireAudioBackend()
    : m_impl(std::make_unique<Impl>())
{
}

PipeWireAudioBackend::~PipeWireAudioBackend()
{
    stop();
}

bool PipeWireAudioBackend::start(const Callbacks &callbacks, QString *errorOut)
{
    if (m_impl->loop)
        return true;
    m_impl->callbacks = callbacks;
    m_impl->state.reset(++m_impl->attemptGeneration);
    static std::once_flag initialized;
    std::call_once(initialized, [] { pw_init(nullptr, nullptr); });

    m_impl->loop = pw_thread_loop_new("astrea-audio", nullptr);
    if (!m_impl->loop) {
        if (errorOut)
            *errorOut = QStringLiteral("PipeWire thread loop could not be created");
        return false;
    }
    m_impl->context = pw_context_new(pw_thread_loop_get_loop(m_impl->loop), nullptr, 0);
    if (!m_impl->context) {
        if (errorOut)
            *errorOut = QStringLiteral("PipeWire context could not be created");
        stop();
        return false;
    }
    m_impl->core = pw_context_connect(m_impl->context, nullptr, 0);
    if (!m_impl->core) {
        if (errorOut)
            *errorOut = QStringLiteral("PipeWire daemon is unavailable");
        stop();
        return false;
    }
    m_impl->registry = pw_core_get_registry(m_impl->core, PW_VERSION_REGISTRY, 0);
    if (!m_impl->registry) {
        if (errorOut)
            *errorOut = QStringLiteral("PipeWire registry is unavailable");
        stop();
        return false;
    }
    m_impl->coreEvents.version = PW_VERSION_CORE_EVENTS;
    m_impl->coreEvents.error = &Impl::coreError;
    pw_core_add_listener(m_impl->core, &m_impl->coreListener, &m_impl->coreEvents,
                         m_impl.get());
    m_impl->coreListenerAdded = true;
    m_impl->registryEvents.version = PW_VERSION_REGISTRY_EVENTS;
    m_impl->registryEvents.global = &Impl::registryGlobal;
    m_impl->registryEvents.global_remove = &Impl::registryGlobalRemove;
    pw_registry_add_listener(m_impl->registry, &m_impl->registryListener,
                             &m_impl->registryEvents, m_impl.get());
    m_impl->registryListenerAdded = true;
    if (pw_thread_loop_start(m_impl->loop) < 0) {
        if (errorOut)
            *errorOut = QStringLiteral("PipeWire thread loop could not start");
        stop();
        return false;
    }
    m_impl->threadStarted = true;
    return true;
}

void PipeWireAudioBackend::stop()
{
    pw_thread_loop *loop = m_impl->loop;
    const bool threadStarted = m_impl->threadStarted;
    if (loop && threadStarted)
        pw_thread_loop_lock(loop);
    m_impl->callbacks = {};

    m_impl->releaseMetadataProxy();
    m_impl->releaseAllNodeProxies();
    m_impl->outputs.clear();
    m_impl->inputs.clear();
    m_impl->defaultInputId = 0;
    m_impl->defaultInputName.clear();

    if (m_impl->registry && m_impl->registryListenerAdded) {
        spa_hook_remove(&m_impl->registryListener);
        m_impl->registryListenerAdded = false;
    }
    if (m_impl->registry) {
        pw_proxy_destroy(reinterpret_cast<pw_proxy *>(m_impl->registry));
        m_impl->registry = nullptr;
    }
    if (m_impl->core && m_impl->coreListenerAdded) {
        spa_hook_remove(&m_impl->coreListener);
        m_impl->coreListenerAdded = false;
    }
    if (m_impl->core) {
        pw_core_disconnect(m_impl->core);
        m_impl->core = nullptr;
    }
    if (m_impl->context) {
        pw_context_destroy(m_impl->context);
        m_impl->context = nullptr;
    }
    if (loop && threadStarted) {
        pw_thread_loop_unlock(loop);
        pw_thread_loop_stop(loop);
    }
    m_impl->threadStarted = false;
    if (loop) {
        pw_thread_loop_destroy(loop);
        m_impl->loop = nullptr;
    }
}

bool PipeWireAudioBackend::setDefaultOutput(quint32 nodeId)
{
    if (!m_impl->loop)
        return false;
    pw_thread_loop_lock(m_impl->loop);
    const bool result = m_impl->state.requestMetadataDefault(
        nodeId, m_impl->metadata != nullptr, [this](const QString &name) {
            QJsonObject object;
            object.insert(QStringLiteral("name"), name);
            const QByteArray value = QJsonDocument(object).toJson(QJsonDocument::Compact);
            return pw_metadata_set_property(m_impl->metadata, PW_ID_CORE, "default.audio.sink",
                                            "Spa:String:JSON", value.constData()) >= 0;
        });
    pw_thread_loop_unlock(m_impl->loop);
    return result;
}

bool PipeWireAudioBackend::setVolume(quint32 nodeId, double linear)
{
    if (!m_impl->loop || !std::isfinite(linear))
        return false;
    pw_thread_loop_lock(m_impl->loop);

    // Audio sinks expose per-channel software volume. Updating only SPA_PROP_volume
    // does not update SPA_PROP_channelVolumes, which is what WirePlumber and our
    // default-volume observer use. Apply the same gain to every known channel.
    const auto binding = m_impl->nodeBindings.find(nodeId);
    const qsizetype count = binding != m_impl->nodeBindings.end()
            && binding->second && !binding->second->channelVolumes.isEmpty()
        ? binding->second->channelVolumes.size() : 0;
    const float gain = static_cast<float>(std::clamp(linear, 0.0, 3.375));
    std::byte buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod *pod = nullptr;
    if (count > 0 && count <= 64) {
        QVector<float> volumes(count, gain);
        pod = static_cast<const spa_pod *>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props,
            SPA_PROP_channelVolumes,
            SPA_POD_Array(sizeof(float), SPA_TYPE_Float,
                          static_cast<int>(volumes.size()), volumes.data())));
    } else if (count == 0) {
        // Nodes that expose only a scalar volume keep their original contract.
        pod = static_cast<const spa_pod *>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props,
            SPA_PROP_volume, SPA_POD_Float(gain)));
    }
    const bool result = pod && m_impl->setProps(nodeId, pod);
    pw_thread_loop_unlock(m_impl->loop);
    return result;
}

bool PipeWireAudioBackend::setMute(quint32 nodeId, bool muted)
{
    if (!m_impl->loop)
        return false;
    std::byte buffer[256];
    pw_thread_loop_lock(m_impl->loop);
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod *pod = static_cast<const spa_pod *>(spa_pod_builder_add_object(
        &builder, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props,
        SPA_PROP_mute, SPA_POD_Bool(muted)));
    const bool result = pod && m_impl->setProps(nodeId, pod);
    pw_thread_loop_unlock(m_impl->loop);
    return result;
}

bool PipeWireAudioBackend::setDefaultInput(quint32 nodeId)
{
    if (!m_impl->loop) return false;
    pw_thread_loop_lock(m_impl->loop);
    const auto it = m_impl->inputs.find(nodeId);
    bool ok = false;
    if (it != m_impl->inputs.end() && m_impl->metadata && !it->second.name.isEmpty()) {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), it->second.name);
        const QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        ok = pw_metadata_set_property(m_impl->metadata, PW_ID_CORE, "default.audio.source",
                                      "Spa:String:JSON", json.constData()) >= 0;
    }
    pw_thread_loop_unlock(m_impl->loop);
    return ok;
}

bool PipeWireAudioBackend::setInputVolume(quint32 nodeId, double linear)
{
    // setVolume locks the PipeWire loop before accessing the node proxy.
    return setVolume(nodeId, linear);
}

bool PipeWireAudioBackend::setInputMute(quint32 nodeId, bool muted)
{
    return setMute(nodeId, muted);
}

} // namespace Astrea::System
