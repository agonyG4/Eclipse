#pragma once

class QString;
struct wl_display;

namespace Astrea::Typhon {

class TyphonShellAuthenticator final {
public:
    static bool authenticate(wl_display *display, QString *diagnostic = nullptr);
};

} // namespace Astrea::Typhon
