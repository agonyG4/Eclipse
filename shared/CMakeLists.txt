cmake_minimum_required(VERSION 3.16)
project(AstreaShared LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(LayerShellQt QUIET)

add_library(astrea-shared-core STATIC
    apps/DesktopEntryParser.cpp
    apps/DesktopEntryParser.hpp
    apps/DesktopEntryCatalog.cpp
    apps/DesktopEntryCatalog.hpp
    launch/ApplicationLauncher.cpp
    launch/ApplicationLauncher.hpp
    icons/AstreaIconTheme.cpp
    icons/AstreaIconTheme.hpp
    icons/AstreaIconProvider.cpp
    icons/AstreaIconProvider.hpp
)

target_include_directories(astrea-shared-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(NOT MSVC)
    target_compile_options(astrea-shared-core PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(astrea-shared-core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

function(astrea_shared_enable_sanitizers target)
    if(NOT MSVC)
        set(flags)
        if(ASTREA_ENABLE_ASAN)
            list(APPEND flags address)
        endif()
        if(ASTREA_ENABLE_UBSAN)
            list(APPEND flags undefined)
        endif()
        if(flags)
            list(JOIN flags "," sanitizer_flags)
            target_compile_options(${target} PRIVATE -fsanitize=${sanitizer_flags} -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=${sanitizer_flags})
        endif()
    endif()
endfunction()

astrea_shared_enable_sanitizers(astrea-shared-core)

qt_add_qml_module(astrea-shared-core
    URI Astrea.Shared
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Astrea/Shared"
    QML_FILES
        qml/AstreaAppIcon.qml
)

add_library(astrea-shared-layer-shell STATIC
    platform/wayland/LayerShellHelper.cpp
    platform/wayland/LayerShellHelper.hpp
)

target_include_directories(astrea-shared-layer-shell PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(NOT MSVC)
    target_compile_options(astrea-shared-layer-shell PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(astrea-shared-layer-shell PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)

if(LayerShellQt_FOUND)
    target_link_libraries(astrea-shared-layer-shell PUBLIC LayerShellQt::Interface)
    target_compile_definitions(astrea-shared-layer-shell PUBLIC ASTREA_HAVE_LAYER_SHELL_QT=1)
else()
    target_compile_definitions(astrea-shared-layer-shell PUBLIC ASTREA_HAVE_LAYER_SHELL_QT=0)
endif()

astrea_shared_enable_sanitizers(astrea-shared-layer-shell)

add_library(astrea-shared-typhon STATIC
    platform/typhon/TyphonProtocolAdapter.cpp
    platform/typhon/TyphonProtocolAdapter.hpp
    platform/typhon/TyphonShellAuthenticator.cpp
    platform/typhon/TyphonShellAuthenticator.hpp
    platform/typhon/TyphonProtocolTypes.hpp
    platform/typhon/TyphonActionState.cpp
    platform/typhon/TyphonActionState.hpp
    platform/typhon/TyphonSharedConnection.cpp
    platform/typhon/TyphonSharedConnection.hpp
    platform/typhon/TyphonWaylandDisplay.cpp
    platform/typhon/TyphonWaylandDisplay.hpp
    platform/typhon/TyphonShortcutClient.cpp
    platform/typhon/TyphonShortcutClient.hpp
    platform/typhon/TyphonToplevelConnection.cpp
    platform/typhon/TyphonToplevelConnection.hpp
    platform/typhon/TyphonToplevelModel.cpp
    platform/typhon/TyphonToplevelModel.hpp
    platform/typhon/TyphonAppMatcher.cpp
    platform/typhon/TyphonAppMatcher.hpp
    platform/typhon/DockApplicationStateProjector.cpp
    platform/typhon/DockApplicationStateProjector.hpp
)

target_include_directories(astrea-shared-typhon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(NOT MSVC)
    target_compile_options(astrea-shared-typhon PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(astrea-shared-typhon PUBLIC Qt6::Core)
if(ASTREA_TYPHON_WAYLAND_AVAILABLE)
    set(ASTREA_TYPHON_PROTOCOL_XML
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/typhon/protocols/astrea-toplevel-management-v1.xml")
    set(ASTREA_TYPHON_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/typhon")
    set(ASTREA_TYPHON_GENERATED_HEADER
        "${ASTREA_TYPHON_GENERATED_DIR}/astrea-toplevel-management-v1-client-protocol.h")
    set(ASTREA_TYPHON_GENERATED_SOURCE
        "${ASTREA_TYPHON_GENERATED_DIR}/astrea-toplevel-management-v1-client-protocol.c")
    set(ASTREA_TYPHON_SERVER_GENERATED_HEADER
        "${ASTREA_TYPHON_GENERATED_DIR}/astrea-toplevel-management-v1-server-protocol.h")
    set(ASTREA_TYPHON_SERVER_GENERATED_SOURCE
        "${ASTREA_TYPHON_GENERATED_DIR}/astrea-toplevel-management-v1-server-protocol.c")
    set(ASTREA_SHORTCUT_PROTOCOL_XML
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/typhon/protocols/astrea-shortcuts-v1.xml")
    set(ASTREA_SHORTCUT_GENERATED_HEADER
        "${ASTREA_TYPHON_GENERATED_DIR}/astrea-shortcuts-v1-client-protocol.h")
    set(ASTREA_SHORTCUT_GENERATED_SOURCE
        "${ASTREA_TYPHON_GENERATED_DIR}/astrea-shortcuts-v1-client-protocol.c")
    set(ASTREA_SHORTCUT_SERVER_GENERATED_HEADER
        "${ASTREA_TYPHON_GENERATED_DIR}/astrea-shortcuts-v1-server-protocol.h")
    set(ASTREA_SHORTCUT_SERVER_GENERATED_SOURCE
        "${ASTREA_TYPHON_GENERATED_DIR}/astrea-shortcuts-v1-server-protocol.c")
    set(ASTREA_SHELL_AUTH_PROTOCOL_XML
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/typhon/protocols/astrea-shell-auth-v1.xml")
    set(ASTREA_SHELL_AUTH_GENERATED_HEADER
        "${ASTREA_TYPHON_GENERATED_DIR}/astrea-shell-auth-v1-client-protocol.h")
    set(ASTREA_SHELL_AUTH_GENERATED_SOURCE
        "${ASTREA_TYPHON_GENERATED_DIR}/astrea-shell-auth-v1-client-protocol.c")
    set(ASTREA_SHELL_AUTH_SERVER_GENERATED_HEADER
        "${ASTREA_TYPHON_GENERATED_DIR}/astrea-shell-auth-v1-server-protocol.h")
    set(ASTREA_SHELL_AUTH_SERVER_GENERATED_SOURCE
        "${ASTREA_TYPHON_GENERATED_DIR}/astrea-shell-auth-v1-server-protocol.c")
    add_custom_command(
        OUTPUT "${ASTREA_TYPHON_GENERATED_HEADER}" "${ASTREA_TYPHON_GENERATED_SOURCE}"
               "${ASTREA_TYPHON_SERVER_GENERATED_HEADER}" "${ASTREA_TYPHON_SERVER_GENERATED_SOURCE}"
               "${ASTREA_SHORTCUT_GENERATED_HEADER}" "${ASTREA_SHORTCUT_GENERATED_SOURCE}"
               "${ASTREA_SHORTCUT_SERVER_GENERATED_HEADER}" "${ASTREA_SHORTCUT_SERVER_GENERATED_SOURCE}"
               "${ASTREA_SHELL_AUTH_GENERATED_HEADER}" "${ASTREA_SHELL_AUTH_GENERATED_SOURCE}"
               "${ASTREA_SHELL_AUTH_SERVER_GENERATED_HEADER}" "${ASTREA_SHELL_AUTH_SERVER_GENERATED_SOURCE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ASTREA_TYPHON_GENERATED_DIR}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" client-header
                "${ASTREA_TYPHON_PROTOCOL_XML}" "${ASTREA_TYPHON_GENERATED_HEADER}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" private-code
                "${ASTREA_TYPHON_PROTOCOL_XML}" "${ASTREA_TYPHON_GENERATED_SOURCE}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" server-header
                "${ASTREA_TYPHON_PROTOCOL_XML}" "${ASTREA_TYPHON_SERVER_GENERATED_HEADER}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" private-code
                "${ASTREA_TYPHON_PROTOCOL_XML}" "${ASTREA_TYPHON_SERVER_GENERATED_SOURCE}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" client-header
                "${ASTREA_SHORTCUT_PROTOCOL_XML}" "${ASTREA_SHORTCUT_GENERATED_HEADER}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" private-code
                "${ASTREA_SHORTCUT_PROTOCOL_XML}" "${ASTREA_SHORTCUT_GENERATED_SOURCE}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" server-header
                "${ASTREA_SHORTCUT_PROTOCOL_XML}" "${ASTREA_SHORTCUT_SERVER_GENERATED_HEADER}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" private-code
                "${ASTREA_SHORTCUT_PROTOCOL_XML}" "${ASTREA_SHORTCUT_SERVER_GENERATED_SOURCE}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" client-header
                "${ASTREA_SHELL_AUTH_PROTOCOL_XML}" "${ASTREA_SHELL_AUTH_GENERATED_HEADER}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" private-code
                "${ASTREA_SHELL_AUTH_PROTOCOL_XML}" "${ASTREA_SHELL_AUTH_GENERATED_SOURCE}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" server-header
                "${ASTREA_SHELL_AUTH_PROTOCOL_XML}" "${ASTREA_SHELL_AUTH_SERVER_GENERATED_HEADER}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" private-code
                "${ASTREA_SHELL_AUTH_PROTOCOL_XML}" "${ASTREA_SHELL_AUTH_SERVER_GENERATED_SOURCE}"
        DEPENDS "${ASTREA_TYPHON_PROTOCOL_XML}" "${ASTREA_SHORTCUT_PROTOCOL_XML}"
                "${ASTREA_SHELL_AUTH_PROTOCOL_XML}"
        VERBATIM
    )
    set_source_files_properties("${ASTREA_TYPHON_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    set_source_files_properties("${ASTREA_TYPHON_SERVER_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    set_source_files_properties("${ASTREA_SHORTCUT_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    set_source_files_properties("${ASTREA_SHORTCUT_SERVER_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    set_source_files_properties("${ASTREA_SHELL_AUTH_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    set_source_files_properties("${ASTREA_SHELL_AUTH_SERVER_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    target_sources(astrea-shared-typhon PRIVATE
        "${ASTREA_TYPHON_GENERATED_HEADER}"
        "${ASTREA_TYPHON_GENERATED_SOURCE}"
        "${ASTREA_SHORTCUT_GENERATED_HEADER}"
        "${ASTREA_SHORTCUT_GENERATED_SOURCE}"
        "${ASTREA_SHELL_AUTH_GENERATED_HEADER}"
        "${ASTREA_SHELL_AUTH_GENERATED_SOURCE}"
        "${ASTREA_SHELL_AUTH_SERVER_GENERATED_HEADER}"
        "${ASTREA_SHELL_AUTH_SERVER_GENERATED_SOURCE}"
    )
    target_include_directories(astrea-shared-typhon PRIVATE "${ASTREA_TYPHON_GENERATED_DIR}")
    target_link_libraries(astrea-shared-typhon PRIVATE PkgConfig::ASTREA_WAYLAND_CLIENT)
    target_compile_definitions(astrea-shared-typhon PUBLIC ASTREA_HAVE_TYPHON_WAYLAND=1)
    target_compile_definitions(astrea-shared-typhon PUBLIC ASTREA_HAVE_TYPHON_PROTOCOL=1)
else()
    target_compile_definitions(astrea-shared-typhon PUBLIC ASTREA_HAVE_TYPHON_WAYLAND=0)
    target_compile_definitions(astrea-shared-typhon PUBLIC ASTREA_HAVE_TYPHON_PROTOCOL=0)
endif()

astrea_shared_enable_sanitizers(astrea-shared-typhon)

option(ASTREA_SHARED_BUILD_TESTS "Build shared foundation tests" ON)
if(BUILD_TESTING AND ASTREA_SHARED_BUILD_TESTS AND ASTREA_BUILD_TESTS)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

    add_executable(typhon-shortcut-protocol-contract-test
        tests/TyphonShortcutProtocolContractTest.cpp
    )
    target_include_directories(typhon-shortcut-protocol-contract-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_definitions(typhon-shortcut-protocol-contract-test PRIVATE
        ASTREA_SHORTCUT_PROTOCOL_SOURCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/platform/typhon/protocols/astrea-shortcuts-v1.xml"
    )
    target_link_libraries(typhon-shortcut-protocol-contract-test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME typhon-shortcut-protocol-contract-test COMMAND typhon-shortcut-protocol-contract-test)

    add_executable(typhon-toplevel-protocol-contract-test
        tests/TyphonToplevelProtocolContractTest.cpp
    )
    target_include_directories(typhon-toplevel-protocol-contract-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_definitions(typhon-toplevel-protocol-contract-test PRIVATE
        ASTREA_TOPLEVEL_PROTOCOL_SOURCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/platform/typhon/protocols/astrea-toplevel-management-v1.xml"
    )
    target_link_libraries(typhon-toplevel-protocol-contract-test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME typhon-toplevel-protocol-contract-test COMMAND typhon-toplevel-protocol-contract-test)

    add_executable(desktop-entry-catalog-test
        tests/DesktopEntryCatalogTest.cpp
        apps/DesktopEntryCatalog.cpp
    )
    target_include_directories(desktop-entry-catalog-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(desktop-entry-catalog-test PRIVATE astrea-shared-core Qt6::Test)
    astrea_shared_enable_sanitizers(desktop-entry-catalog-test)
    add_test(NAME desktop-entry-catalog-test COMMAND desktop-entry-catalog-test)

    function(add_typhon_shared_test TEST_NAME TEST_FILE)
        add_executable(${TEST_NAME}
            ${TEST_FILE}
        )
        target_include_directories(${TEST_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_link_libraries(${TEST_NAME} PRIVATE astrea-shared-typhon Qt6::Test)
        astrea_shared_enable_sanitizers(${TEST_NAME})
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endfunction()

    add_typhon_shared_test(typhon-toplevel-model-test tests/TyphonToplevelModelTest.cpp)
    add_typhon_shared_test(typhon-app-matcher-test tests/TyphonAppMatcherTest.cpp)
    add_typhon_shared_test(dock-application-state-projector-test tests/DockApplicationStateProjectorTest.cpp)
    add_typhon_shared_test(typhon-protocol-client-test tests/TyphonProtocolClientTest.cpp)
    add_typhon_shared_test(typhon-action-state-test tests/TyphonActionStateTest.cpp)
    add_typhon_shared_test(typhon-shared-connection-test tests/TyphonSharedConnectionTest.cpp)

    if(ASTREA_TYPHON_WAYLAND_AVAILABLE AND ASTREA_WAYLAND_SERVER_FOUND)
        add_executable(typhon-protocol-integration-test
            tests/TyphonProtocolIntegrationTest.cpp
            "${ASTREA_TYPHON_SERVER_GENERATED_SOURCE}"
            "${ASTREA_SHORTCUT_SERVER_GENERATED_SOURCE}"
            "${ASTREA_SHELL_AUTH_SERVER_GENERATED_SOURCE}"
        )
        target_include_directories(typhon-protocol-integration-test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${ASTREA_TYPHON_GENERATED_DIR}"
        )
        target_link_libraries(typhon-protocol-integration-test PRIVATE
            astrea-shared-typhon
            Qt6::Test
            PkgConfig::ASTREA_WAYLAND_SERVER
        )
        astrea_shared_enable_sanitizers(typhon-protocol-integration-test)
        add_test(NAME typhon-protocol-integration-test COMMAND typhon-protocol-integration-test)
    else()
        add_test(NAME typhon-protocol-integration-test
            COMMAND ${CMAKE_COMMAND} -E echo
                    "SKIPPED: Wayland client/server development files are unavailable")
        set_tests_properties(typhon-protocol-integration-test PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIPPED")
    endif()

    if(ASTREA_TYPHON_WAYLAND_AVAILABLE AND ASTREA_WAYLAND_SERVER_FOUND)
        add_executable(typhon-shortcut-protocol-integration-test
            tests/TyphonShortcutProtocolIntegrationTest.cpp
            "${ASTREA_SHORTCUT_SERVER_GENERATED_SOURCE}"
            "${ASTREA_SHELL_AUTH_SERVER_GENERATED_SOURCE}"
        )
        target_include_directories(typhon-shortcut-protocol-integration-test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${ASTREA_TYPHON_GENERATED_DIR}"
        )
        target_link_libraries(typhon-shortcut-protocol-integration-test PRIVATE
            astrea-shared-typhon
            Qt6::Test
            PkgConfig::ASTREA_WAYLAND_SERVER
        )
        astrea_shared_enable_sanitizers(typhon-shortcut-protocol-integration-test)
        add_test(NAME typhon-shortcut-protocol-integration-test
                 COMMAND typhon-shortcut-protocol-integration-test)
    else()
        add_test(NAME typhon-shortcut-protocol-integration-test
            COMMAND ${CMAKE_COMMAND} -E echo
                    "SKIPPED: Wayland client/server development files are unavailable")
        set_tests_properties(typhon-shortcut-protocol-integration-test PROPERTIES
            SKIP_REGULAR_EXPRESSION "SKIPPED")
    endif()

    if(ASTREA_ENABLE_ASAN)
        set_tests_properties(
            desktop-entry-catalog-test
            typhon-toplevel-protocol-contract-test
            typhon-toplevel-model-test
            typhon-app-matcher-test
            dock-application-state-projector-test
            typhon-protocol-client-test
            typhon-action-state-test
            typhon-protocol-integration-test
            typhon-shortcut-protocol-integration-test
            PROPERTIES
            ENVIRONMENT
                "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/AltTab/lsan.supp:print_suppressions=0;ASAN_OPTIONS=detect_leaks=1:halt_on_error=1;UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
        )
    endif()
endif()
