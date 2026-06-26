#include "platform/compositor/FakeBackend.hpp"

// FakeWindowSource is a concrete CompositorBackend used in tests.
// This translation unit forces the compiler to emit its vtable
// in exactly one place, preventing "undefined reference to vtable" linker errors.
