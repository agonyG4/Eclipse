cmake_minimum_required(VERSION 3.16)
project(AstreaShared LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick DBus)
if(NOT TARGET PkgConfig::ASTREA_PIPEWIRE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ASTREA_PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
endif()

add_library(astrea-shared-core STATIC
    apps/DesktopEntryParser.cpp
    apps/DesktopEntryParser.hpp
    apps/DesktopEntryCatalog.cpp
    apps/DesktopEntryCatalog.hpp
    launch/ApplicationLauncher.cpp
    launch/ApplicationLauncher.hpp
    theme/ThemeController.cpp
    theme/ThemeController.hpp
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

add_library(astrea-shared-statusnotifier STATIC
    statusnotifier/StatusNotifierTypes.cpp
    statusnotifier/StatusNotifierTypes.hpp
    statusnotifier/StatusNotifierIconStore.cpp
    statusnotifier/StatusNotifierIconStore.hpp
    statusnotifier/StatusNotifierIconProvider.cpp
    statusnotifier/StatusNotifierIconProvider.hpp
    statusnotifier/StatusNotifierItemModel.cpp
    statusnotifier/StatusNotifierItemModel.hpp
    statusnotifier/DBusMenuModel.cpp
    statusnotifier/DBusMenuModel.hpp
    statusnotifier/StatusNotifierWatcherBridge.cpp
    statusnotifier/StatusNotifierWatcherBridge.hpp
    statusnotifier/StatusNotifierItemProxy.cpp
    statusnotifier/StatusNotifierItemProxy.hpp
    statusnotifier/StatusNotifierService.cpp
    statusnotifier/StatusNotifierService.hpp
)

target_include_directories(astrea-shared-statusnotifier PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(astrea-shared-statusnotifier PUBLIC
    astrea-shared-core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::DBus
)

if(NOT MSVC)
    target_compile_options(astrea-shared-statusnotifier PRIVATE -Wall -Wextra -Wpedantic)
endif()

astrea_shared_enable_sanitizers(astrea-shared-statusnotifier)

add_library(astrea-shared-system STATIC
    system/SystemServiceState.cpp
    system/SystemServiceState.hpp
    system/audio/AudioOutputModel.cpp
    system/audio/AudioOutputModel.hpp
    system/audio/AudioBackend.hpp
    system/audio/AudioService.cpp
    system/audio/AudioService.hpp
    system/audio/PipeWireAudioState.cpp
    system/audio/PipeWireAudioState.hpp
    system/audio/PipeWireAudioBackend.cpp
    system/audio/PipeWireAudioBackend.hpp
    system/network/WifiNetworkModel.cpp
    system/network/WifiNetworkModel.hpp
    system/network/NetworkBackend.hpp
    system/network/NetworkService.cpp
    system/network/NetworkService.hpp
    system/network/NetworkManagerState.cpp
    system/network/NetworkManagerState.hpp
    system/network/NetworkManagerBackend.cpp
    system/network/NetworkManagerBackend.hpp
    system/bluetooth/BluetoothDeviceModel.cpp
    system/bluetooth/BluetoothDeviceModel.hpp
    system/bluetooth/BluetoothBackend.hpp
    system/bluetooth/BluetoothService.cpp
    system/bluetooth/BluetoothService.hpp
    system/bluetooth/BluezObjectStore.cpp
    system/bluetooth/BluezObjectStore.hpp
    system/bluetooth/BluezDiscoveryState.cpp
    system/bluetooth/BluezDiscoveryState.hpp
    system/bluetooth/BluezBackend.cpp
    system/bluetooth/BluezBackend.hpp
)

target_include_directories(astrea-shared-system PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(astrea-shared-system PUBLIC
    Qt6::Core
    Qt6::DBus
    PkgConfig::ASTREA_PIPEWIRE
)

if(NOT MSVC)
    target_compile_options(astrea-shared-system PRIVATE -Wall -Wextra -Wpedantic)
endif()

astrea_shared_enable_sanitizers(astrea-shared-system)

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

if(ASTREA_ENABLE_LAYER_SHELL)
    if(NOT TARGET LayerShellQt::Interface)
        message(FATAL_ERROR
            "astrea-shared-layer-shell requires LayerShellQt::Interface when "
            "ASTREA_ENABLE_LAYER_SHELL=ON")
    endif()
    if(NOT TARGET PkgConfig::ASTREA_WAYLAND_CLIENT)
        message(FATAL_ERROR
            "astrea-shared-layer-shell requires the wayland-client development target when "
            "ASTREA_ENABLE_LAYER_SHELL=ON; install Wayland client development files or use "
            "-DASTREA_ENABLE_LAYER_SHELL=OFF for non-production/test builds")
    endif()
    target_link_libraries(astrea-shared-layer-shell PUBLIC
        LayerShellQt::Interface
        PkgConfig::ASTREA_WAYLAND_CLIENT
    )
    target_compile_definitions(astrea-shared-layer-shell PUBLIC ASTREA_HAVE_LAYER_SHELL_QT=1)
    if(ASTREA_LAYER_SHELL_HAS_ACTIVATE_ON_SHOW)
        target_compile_definitions(astrea-shared-layer-shell PUBLIC
            ASTREA_LAYER_SHELL_HAS_ACTIVATE_ON_SHOW=1)
    else()
        target_compile_definitions(astrea-shared-layer-shell PUBLIC
            ASTREA_LAYER_SHELL_HAS_ACTIVATE_ON_SHOW=0)
    endif()
else()
    target_compile_definitions(astrea-shared-layer-shell PUBLIC ASTREA_HAVE_LAYER_SHELL_QT=0)
    target_compile_definitions(astrea-shared-layer-shell PUBLIC
        ASTREA_LAYER_SHELL_HAS_ACTIVATE_ON_SHOW=0)
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
    platform/typhon/TyphonWorkspaceState.cpp
    platform/typhon/TyphonWorkspaceState.hpp
    platform/typhon/TyphonWorkspaceClient.cpp
    platform/typhon/TyphonWorkspaceClient.hpp
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
    set(ASTREA_EXT_WORKSPACE_PROTOCOL_XML
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/typhon/protocols/ext-workspace-v1.xml")
    set(ASTREA_EXT_WORKSPACE_GENERATED_HEADER
        "${ASTREA_TYPHON_GENERATED_DIR}/ext-workspace-v1-client-protocol.h")
    set(ASTREA_EXT_WORKSPACE_GENERATED_SOURCE
        "${ASTREA_TYPHON_GENERATED_DIR}/ext-workspace-v1-protocol.c")
    add_custom_command(
        OUTPUT "${ASTREA_TYPHON_GENERATED_HEADER}" "${ASTREA_TYPHON_GENERATED_SOURCE}"
               "${ASTREA_TYPHON_SERVER_GENERATED_HEADER}" "${ASTREA_TYPHON_SERVER_GENERATED_SOURCE}"
               "${ASTREA_SHORTCUT_GENERATED_HEADER}" "${ASTREA_SHORTCUT_GENERATED_SOURCE}"
               "${ASTREA_SHORTCUT_SERVER_GENERATED_HEADER}" "${ASTREA_SHORTCUT_SERVER_GENERATED_SOURCE}"
               "${ASTREA_SHELL_AUTH_GENERATED_HEADER}" "${ASTREA_SHELL_AUTH_GENERATED_SOURCE}"
               "${ASTREA_SHELL_AUTH_SERVER_GENERATED_HEADER}" "${ASTREA_SHELL_AUTH_SERVER_GENERATED_SOURCE}"
               "${ASTREA_EXT_WORKSPACE_GENERATED_HEADER}" "${ASTREA_EXT_WORKSPACE_GENERATED_SOURCE}"
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
        COMMAND "${ASTREA_WAYLAND_SCANNER}" client-header
                "${ASTREA_EXT_WORKSPACE_PROTOCOL_XML}" "${ASTREA_EXT_WORKSPACE_GENERATED_HEADER}"
        COMMAND "${ASTREA_WAYLAND_SCANNER}" private-code
                "${ASTREA_EXT_WORKSPACE_PROTOCOL_XML}" "${ASTREA_EXT_WORKSPACE_GENERATED_SOURCE}"
        DEPENDS "${ASTREA_TYPHON_PROTOCOL_XML}" "${ASTREA_SHORTCUT_PROTOCOL_XML}"
                "${ASTREA_SHELL_AUTH_PROTOCOL_XML}" "${ASTREA_EXT_WORKSPACE_PROTOCOL_XML}"
        VERBATIM
    )
    set_source_files_properties("${ASTREA_TYPHON_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    set_source_files_properties("${ASTREA_TYPHON_SERVER_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    set_source_files_properties("${ASTREA_SHORTCUT_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    set_source_files_properties("${ASTREA_SHORTCUT_SERVER_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    set_source_files_properties("${ASTREA_SHELL_AUTH_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    set_source_files_properties("${ASTREA_SHELL_AUTH_SERVER_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    set_source_files_properties("${ASTREA_EXT_WORKSPACE_GENERATED_SOURCE}" PROPERTIES LANGUAGE C)
    target_sources(astrea-shared-typhon PRIVATE
        "${ASTREA_TYPHON_GENERATED_HEADER}"
        "${ASTREA_TYPHON_GENERATED_SOURCE}"
        "${ASTREA_SHORTCUT_GENERATED_HEADER}"
        "${ASTREA_SHORTCUT_GENERATED_SOURCE}"
        "${ASTREA_SHELL_AUTH_GENERATED_HEADER}"
        "${ASTREA_SHELL_AUTH_GENERATED_SOURCE}"
        "${ASTREA_SHELL_AUTH_SERVER_GENERATED_HEADER}"
        "${ASTREA_SHELL_AUTH_SERVER_GENERATED_SOURCE}"
        "${ASTREA_EXT_WORKSPACE_GENERATED_HEADER}"
        "${ASTREA_EXT_WORKSPACE_GENERATED_SOURCE}"
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
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

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

    add_executable(system-services-test
        system/tests/SystemServicesTest.cpp
    )
    target_include_directories(system-services-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(system-services-test PRIVATE
        astrea-shared-system
        Qt6::Test
    )
    astrea_shared_enable_sanitizers(system-services-test)
    add_test(NAME system-services-test COMMAND system-services-test)

    add_executable(statusnotifier-test
        tests/StatusNotifierTest.cpp
    )
    target_include_directories(statusnotifier-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(statusnotifier-test PRIVATE
        astrea-shared-statusnotifier
        Qt6::Test
    )
    astrea_shared_enable_sanitizers(statusnotifier-test)
    add_test(NAME statusnotifier-test COMMAND statusnotifier-test)

    add_executable(statusnotifier-dbus-fixture
        tests/StatusNotifierDBusFixture.cpp
    )
    target_include_directories(statusnotifier-dbus-fixture PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(statusnotifier-dbus-fixture PRIVATE
        astrea-shared-statusnotifier
        Qt6::Core
        Qt6::DBus
    )
    astrea_shared_enable_sanitizers(statusnotifier-dbus-fixture)

    add_executable(statusnotifier-dbus-integration-test
        tests/StatusNotifierIntegrationTest.cpp
    )
    target_include_directories(statusnotifier-dbus-integration-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(statusnotifier-dbus-integration-test PRIVATE
        astrea-shared-statusnotifier
        Qt6::Test
    )
    target_compile_definitions(statusnotifier-dbus-integration-test PRIVATE
        ASTREA_STATUSNOTIFIER_FIXTURE_PATH="${CMAKE_CURRENT_BINARY_DIR}/statusnotifier-dbus-fixture"
    )
    add_dependencies(statusnotifier-dbus-integration-test statusnotifier-dbus-fixture)
    astrea_shared_enable_sanitizers(statusnotifier-dbus-integration-test)
    find_program(ASTREA_DBUS_RUN_SESSION dbus-run-session)
    if(ASTREA_DBUS_RUN_SESSION)
        add_test(NAME statusnotifier-dbus-integration-test
            COMMAND "${ASTREA_DBUS_RUN_SESSION}" --
                    "$<TARGET_FILE:statusnotifier-dbus-integration-test>")
    endif()

    add_executable(layer-shell-helper-test
        tests/LayerShellHelperTest.cpp
    )
    target_include_directories(layer-shell-helper-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(layer-shell-helper-test PRIVATE
        astrea-shared-layer-shell
        Qt6::Test
    )
    astrea_shared_enable_sanitizers(layer-shell-helper-test)
    add_test(NAME layer-shell-helper-test COMMAND layer-shell-helper-test)

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
    add_typhon_shared_test(typhon-workspace-state-test tests/TyphonWorkspaceStateTest.cpp)

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
