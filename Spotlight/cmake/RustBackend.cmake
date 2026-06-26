function(astrea_add_rust_backend target_name)
    set(options)
    set(oneValueArgs MANIFEST_PATH TARGET_DIR HEADER_DIR)
    cmake_parse_arguments(ASTREA "${options}" "${oneValueArgs}" "" ${ARGN})

    find_program(CARGO_EXECUTABLE cargo REQUIRED)

    if(NOT ASTREA_TARGET_DIR)
        set(ASTREA_TARGET_DIR "${CMAKE_BINARY_DIR}/cargo-target")
    endif()

    if(NOT ASTREA_HEADER_DIR)
        set(ASTREA_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/backend/include")
    endif()

    file(GLOB_RECURSE ASTREA_RUST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/backend/src/*.rs"
    )
    set(ASTREA_RUST_INPUTS
        ${ASTREA_RUST_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/backend/Cargo.toml"
        "${CMAKE_CURRENT_SOURCE_DIR}/backend/Cargo.lock"
    )

    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/libastrea_spotlight_backend.a"
        COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${ASTREA_TARGET_DIR}
                ${CARGO_EXECUTABLE} build --manifest-path "${ASTREA_MANIFEST_PATH}" --release
        COMMAND ${CMAKE_COMMAND} -E copy
                "${ASTREA_TARGET_DIR}/release/libastrea_spotlight_backend.a"
                "${CMAKE_BINARY_DIR}/libastrea_spotlight_backend.a"
        DEPENDS ${ASTREA_RUST_INPUTS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building Rust spotlight backend"
        VERBATIM
    )

    add_custom_target(${target_name} ALL
        DEPENDS "${CMAKE_BINARY_DIR}/libastrea_spotlight_backend.a"
    )

    add_library(astrea_spotlight_backend STATIC IMPORTED GLOBAL)
    set_target_properties(astrea_spotlight_backend PROPERTIES
        IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/libastrea_spotlight_backend.a"
        INTERFACE_INCLUDE_DIRECTORIES "${ASTREA_HEADER_DIR}"
    )
    add_dependencies(astrea_spotlight_backend ${target_name})
endfunction()
