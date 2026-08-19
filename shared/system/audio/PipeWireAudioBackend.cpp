#include "system/audio/PipeWireAudioBackend.hpp"

#include <pipewire/core.h>
#include <pipewire/extensions/metadata.h>
#include <pipewire/keys.h>
#include <pipewire/node.h>
#include <pipewire/pipewire.h>
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
    spa_hook registryListener{};
    spa_hook coreListener{};
    spa_hook metadataListener{};
    pw_core_events coreEvents{};
    pw_registry_events registryEvents{};
    pw_metadata_events metadataEvents{};
    Callbacks callbacks;
    std::map<quint32, AudioOutput> outputs;
    std::map<quint32, pw_node *> nodes;
    std::map<quint32, std::unique_ptr<NodeBinding>> nodeBindings;
    QString defaultName;
    quint32 metadataDefaultId = 0;

    quint32 defaultNodeId() const
    {
        if (metadataDefaultId && outputs.contains(metadataDefaultId))
            return metadataDefaultId;
        for (const auto &[id, output] : outputs) {
            if (!defaultName.isEmpty()
                && (output.name == defaultName || output.nick == defaultName))
                return id;
        }
        return outputs.empty() ? 0 : outputs.cbegin()->first;
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
        if (!mediaClass || std::strcmp(mediaClass, "Audio/Sink") != 0)
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
        impl->outputs[id] = output;
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
        impl->publishOutputs();
        impl->publishDefaultVolume();
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
        if (id == impl->metadataId) {
            spa_hook_remove(&impl->metadataListener);
            if (impl->metadata && impl->core)
                pw_core_destroy(impl->core, impl->metadata);
            impl->metadata = nullptr;
            impl->metadataId = 0;
            return;
        }
        const auto nodeIt = impl->nodes.find(id);
        if (nodeIt != impl->nodes.end() && nodeIt->second && impl->core)
            pw_core_destroy(impl->core, nodeIt->second);
        impl->nodes.erase(id);
        impl->nodeBindings.erase(id);
        impl->outputs.erase(id);
        impl->publishOutputs();
    }

    static int metadataProperty(void *data, uint32_t, const char *key,
                                const char *, const char *value)
    {
        auto *impl = static_cast<Impl *>(data);
        if (!key || std::strcmp(key, "default.audio.sink") != 0)
            return 0;
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
        if (it == binding->owner->outputs.end())
            return;
        const char *name = spa_dict_lookup(info->props, PW_KEY_NODE_NAME);
        const char *description = spa_dict_lookup(info->props, PW_KEY_NODE_DESCRIPTION);
        const char *nick = spa_dict_lookup(info->props, PW_KEY_NODE_NICK);
        if (name)
            it->second.name = QString::fromUtf8(name);
        if (description)
            it->second.description = QString::fromUtf8(description);
        if (nick)
            it->second.nick = QString::fromUtf8(nick);
        binding->owner->publishOutputs();
    }

    static void nodeParam(void *data, int, uint32_t id, uint32_t, uint32_t,
                          const spa_pod *param)
    {
        if (id != SPA_PARAM_Props || !param || !spa_pod_is_object(param))
            return;
        auto *binding = static_cast<NodeBinding *>(data);
        if (!binding->owner->callbacks.volumeChanged)
            return;
        const auto *object = reinterpret_cast<const spa_pod_object *>(param);
        const spa_pod_prop *volumeProperty =
            spa_pod_object_find_prop(object, nullptr, SPA_PROP_volume);
        const spa_pod_prop *muteProperty =
            spa_pod_object_find_prop(object, nullptr, SPA_PROP_mute);
        if (volumeProperty)
            spa_pod_get_float(&volumeProperty->value, &binding->volume);
        if (muteProperty)
            spa_pod_get_bool(&muteProperty->value, &binding->muted);
        binding->volumeKnown = volumeProperty != nullptr;
        binding->muteKnown = muteProperty != nullptr;
        const spa_pod_prop *channelProperty =
            spa_pod_object_find_prop(object, nullptr, SPA_PROP_channelVolumes);
        if (channelProperty) {
            uint32_t count = 0;
            const auto *values = static_cast<const float *>(
                spa_pod_get_array(&channelProperty->value, &count));
            if (values && count > 0) {
                binding->channelVolumes.clear();
                binding->channelVolumes.reserve(static_cast<qsizetype>(count));
                for (uint32_t index = 0; index < count; ++index)
                    binding->channelVolumes.append(values[index]);
            }
        }
        if (binding->id == binding->owner->defaultNodeId())
            binding->owner->publishDefaultVolume();
    }

    void publishOutputs()
    {
        if (!callbacks.outputsChanged)
            return;
        QVector<AudioOutput> values;
        values.reserve(static_cast<qsizetype>(outputs.size()));
        const quint32 defaultNodeId = this->defaultNodeId();
        for (const auto &[id, output] : outputs) {
            AudioOutput copy = output;
            copy.isDefault = id == defaultNodeId;
            values.append(std::move(copy));
        }
        callbacks.outputsChanged(std::move(values), defaultNodeId);
    }

    void publishDefaultVolume()
    {
        if (!callbacks.volumeChanged)
            return;
        const auto binding = nodeBindings.find(defaultNodeId());
        if (binding == nodeBindings.end())
            return;
        const NodeBinding &state = *binding->second;
        if (!state.volumeKnown && state.channelVolumes.isEmpty() && !state.muteKnown)
            return;
        float volume = state.volume;
        if (!state.channelVolumes.isEmpty()) {
            double total = 0.0;
            for (const float channel : state.channelVolumes)
                total += channel;
            volume = static_cast<float>(total / state.channelVolumes.size());
        }
        callbacks.volumeChanged(std::max(0.0f, volume), state.muted);
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
    m_impl->registryEvents.version = PW_VERSION_REGISTRY_EVENTS;
    m_impl->registryEvents.global = &Impl::registryGlobal;
    m_impl->registryEvents.global_remove = &Impl::registryGlobalRemove;
    pw_registry_add_listener(m_impl->registry, &m_impl->registryListener,
                             &m_impl->registryEvents, m_impl.get());
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
    if (!m_impl->loop)
        return;
    if (m_impl->threadStarted)
        pw_thread_loop_lock(m_impl->loop);
    m_impl->callbacks = {};
    if (m_impl->registry)
        spa_hook_remove(&m_impl->registryListener);
    if (m_impl->core)
        spa_hook_remove(&m_impl->coreListener);
    if (m_impl->metadata)
        spa_hook_remove(&m_impl->metadataListener);
    if (m_impl->metadata && m_impl->core)
        pw_core_destroy(m_impl->core, m_impl->metadata);
    m_impl->metadata = nullptr;
    m_impl->metadataId = 0;
    for (auto &[id, binding] : m_impl->nodeBindings) {
        Q_UNUSED(id)
        if (binding)
            spa_hook_remove(&binding->listener);
    }
    m_impl->nodeBindings.clear();
    for (const auto &[id, node] : m_impl->nodes) {
        Q_UNUSED(id)
        if (node && m_impl->core)
            pw_core_destroy(m_impl->core, node);
    }
    m_impl->nodes.clear();
    m_impl->outputs.clear();
    if (m_impl->threadStarted) {
        pw_thread_loop_unlock(m_impl->loop);
        pw_thread_loop_stop(m_impl->loop);
        m_impl->threadStarted = false;
    }
    if (m_impl->core)
        pw_core_disconnect(m_impl->core);
    if (m_impl->context)
        pw_context_destroy(m_impl->context);
    pw_thread_loop_destroy(m_impl->loop);
    m_impl->loop = nullptr;
    m_impl->context = nullptr;
    m_impl->core = nullptr;
    m_impl->registry = nullptr;
    m_impl->metadata = nullptr;
    m_impl->metadataId = 0;
}

bool PipeWireAudioBackend::setDefaultOutput(quint32 nodeId)
{
    if (!m_impl->loop)
        return false;
    pw_thread_loop_lock(m_impl->loop);
    const auto it = m_impl->outputs.find(nodeId);
    if (it == m_impl->outputs.end()) {
        pw_thread_loop_unlock(m_impl->loop);
        return false;
    }
    m_impl->defaultName = it->second.name;
    m_impl->metadataDefaultId = nodeId;
    if (m_impl->metadata) {
        const QByteArray value = QByteArray("{\"name\":\"")
            + it->second.name.toUtf8() + QByteArray("\"}");
        pw_metadata_set_property(m_impl->metadata, PW_ID_CORE, "default.audio.sink",
                                 "Spa:String:JSON", value.constData());
    }
    m_impl->publishOutputs();
    pw_thread_loop_unlock(m_impl->loop);
    return true;
}

bool PipeWireAudioBackend::setVolume(quint32 nodeId, double linear)
{
    if (!m_impl->loop)
        return false;
    std::byte buffer[256];
    pw_thread_loop_lock(m_impl->loop);
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const spa_pod *pod = static_cast<const spa_pod *>(spa_pod_builder_add_object(
        &builder, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props,
        SPA_PROP_volume, SPA_POD_Float(static_cast<float>(std::clamp(linear, 0.0, 3.375)))));
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

} // namespace Astrea::System
