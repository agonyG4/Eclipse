// Settings is the CXX-Qt aggregate for its dependent shared system bridge.
// Keep the dependency linked into the aggregate static library so its C++
// bridge objects and initializers are available to CMake consumers.
extern crate astrea_system_backend;

pub mod animation;
pub mod appearance;
pub mod icons;
pub mod theme_config;
pub mod typhon;
pub mod visual_effects;
