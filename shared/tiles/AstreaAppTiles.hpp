#pragma once

namespace AstreaTiles {

// Installs the Rust tile renderer behind the icon provider's tile hook. Call
// once at startup, before QML loads icons.
void installRustRenderer();

} // namespace AstreaTiles
