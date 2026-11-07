# Rust tile renderer (shared/tiles) and the Qt glue that installs it behind the
# icon provider's tile hook. Built only for the full desktop, never the SDK.
find_program(CARGO_EXECUTABLE cargo REQUIRED)

set(ASTREA_APP_TILES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tiles")
set(ASTREA_APP_TILES_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo-target")
set(ASTREA_APP_TILES_LIB "${CMAKE_CURRENT_BINARY_DIR}/libastrea_app_tiles.a")
file(GLOB_RECURSE ASTREA_APP_TILES_SOURCES CONFIGURE_DEPENDS "${ASTREA_APP_TILES_DIR}/src/*.rs")

add_custom_command(
    OUTPUT "${ASTREA_APP_TILES_LIB}"
    COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${ASTREA_APP_TILES_TARGET_DIR}
            ${CARGO_EXECUTABLE} build --manifest-path "${ASTREA_APP_TILES_DIR}/Cargo.toml" --release --lib
    COMMAND ${CMAKE_COMMAND} -E copy
            "${ASTREA_APP_TILES_TARGET_DIR}/release/libastrea_app_tiles.a" "${ASTREA_APP_TILES_LIB}"
    DEPENDS ${ASTREA_APP_TILES_SOURCES} "${ASTREA_APP_TILES_DIR}/Cargo.toml"
    WORKING_DIRECTORY "${ASTREA_APP_TILES_DIR}"
    COMMENT "Building Rust app tile renderer"
    VERBATIM
)
add_custom_target(astrea_app_tiles_rust DEPENDS "${ASTREA_APP_TILES_LIB}")

add_library(astrea_app_tiles_rust_lib STATIC IMPORTED GLOBAL)
set_target_properties(astrea_app_tiles_rust_lib PROPERTIES
    IMPORTED_LOCATION "${ASTREA_APP_TILES_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${ASTREA_APP_TILES_DIR}/include"
)
add_dependencies(astrea_app_tiles_rust_lib astrea_app_tiles_rust)

add_library(astrea-shared-app-tiles STATIC
    tiles/AstreaAppTiles.cpp
    tiles/AstreaAppTiles.hpp
)
target_include_directories(astrea-shared-app-tiles PUBLIC "${ASTREA_APP_TILES_DIR}")
target_link_libraries(astrea-shared-app-tiles PUBLIC
    astrea-shared-core
    astrea_app_tiles_rust_lib
    dl
    pthread
)
if(NOT MSVC)
    target_compile_options(astrea-shared-app-tiles PRIVATE -Wall -Wextra -Wpedantic)
endif()
