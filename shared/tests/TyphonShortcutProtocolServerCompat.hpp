#pragma once

#include <cstdint>

#include <wayland-server-core.h>

struct astrea_shortcuts_manager_v1_interface {
    void (*destroy)(struct wl_client *, struct wl_resource *);
    void (*register_shortcut)(struct wl_client *, struct wl_resource *, std::uint32_t,
                              const char *, const char *, const char *);
};

struct astrea_shortcut_v1_interface {
    void (*destroy)(struct wl_client *, struct wl_resource *);
};

extern "C" {
extern const struct wl_interface astrea_shortcuts_manager_v1_interface;
extern const struct wl_interface astrea_shortcut_v1_interface;
}

constexpr std::uint32_t ASTREA_SHORTCUT_V1_PRESSED = 0;
constexpr std::uint32_t ASTREA_SHORTCUT_V1_REPEATED = 1;
constexpr std::uint32_t ASTREA_SHORTCUT_V1_RELEASED = 2;
constexpr std::uint32_t ASTREA_SHORTCUT_V1_CANCELLED = 3;

inline void astrea_shortcut_v1_send_pressed(struct wl_resource *resource, std::uint32_t serial,
                                            std::uint32_t timestamp)
{
    wl_resource_post_event(resource, ASTREA_SHORTCUT_V1_PRESSED, serial, timestamp);
}

inline void astrea_shortcut_v1_send_repeated(struct wl_resource *resource, std::uint32_t serial,
                                             std::uint32_t timestamp)
{
    wl_resource_post_event(resource, ASTREA_SHORTCUT_V1_REPEATED, serial, timestamp);
}

inline void astrea_shortcut_v1_send_released(struct wl_resource *resource, std::uint32_t serial,
                                             std::uint32_t timestamp)
{
    wl_resource_post_event(resource, ASTREA_SHORTCUT_V1_RELEASED, serial, timestamp);
}

inline void astrea_shortcut_v1_send_cancelled(struct wl_resource *resource, std::uint32_t serial)
{
    wl_resource_post_event(resource, ASTREA_SHORTCUT_V1_CANCELLED, serial);
}
