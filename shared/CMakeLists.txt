cmake_minimum_required(VERSION 3.16)
project(AstreaShared LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(LayerShellQt QUIET)

add_library(astrea-shared-core STATIC
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
    platform/typhon/TyphonProtocolTypes.hpp
    platform/typhon/TyphonWaylandDisplay.cpp
    platform/typhon/TyphonWaylandDisplay.hpp
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
    target_link_libraries(astrea-shared-typhon PRIVATE PkgConfig::ASTREA_WAYLAND_CLIENT)
    target_compile_definitions(astrea-shared-typhon PUBLIC ASTREA_HAVE_TYPHON_WAYLAND=1)
else()
    target_compile_definitions(astrea-shared-typhon PUBLIC ASTREA_HAVE_TYPHON_WAYLAND=0)
endif()
# The generated protocol adapter remains disabled until the finalized Typhon XML
# is copied from its clean M5 commit.
target_compile_definitions(astrea-shared-typhon PUBLIC ASTREA_HAVE_TYPHON_PROTOCOL=0)

astrea_shared_enable_sanitizers(astrea-shared-typhon)

option(ASTREA_SHARED_BUILD_TESTS "Build shared foundation tests" ON)
if(BUILD_TESTING AND ASTREA_SHARED_BUILD_TESTS AND ASTREA_BUILD_TESTS)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)
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

    if(ASTREA_ENABLE_ASAN)
        set_tests_properties(desktop-entry-catalog-test PROPERTIES
            ENVIRONMENT
                "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/AltTab/lsan.supp:print_suppressions=0;ASAN_OPTIONS=detect_leaks=1:halt_on_error=1;UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
        )
    endif()
endif()
