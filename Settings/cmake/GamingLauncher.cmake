# The launcher shares the same native domain crate as the Settings QObject
# projections. Corrosion reuses the build tree's existing Cargo target directory.
corrosion_import_crate(
    MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/gaming/Cargo.toml"
    CRATES astrea_gaming
    CRATE_TYPES bin
    LOCKED
)
set_target_properties(astrea-gaming PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/app"
)
corrosion_install(TARGETS astrea-gaming RUNTIME DESTINATION bin)
