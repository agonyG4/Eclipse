cmake_minimum_required(VERSION 3.16)
project(AstreaShared LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(LayerShellQt QUIET)

add_library(astrea-shared STATIC
    apps/DesktopEntryCatalog.cpp
    apps/DesktopEntryCatalog.hpp
    launch/ApplicationLauncher.cpp
    launch/ApplicationLauncher.hpp
    platform/wayland/LayerShellHelper.cpp
    platform/wayland/LayerShellHelper.hpp
    icons/AstreaIconTheme.cpp
    icons/AstreaIconTheme.hpp
    icons/AstreaIconProvider.cpp
    icons/AstreaIconProvider.hpp
)

target_include_directories(astrea-shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(NOT MSVC)
    target_compile_options(astrea-shared PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(astrea-shared PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

if(LayerShellQt_FOUND)
    target_link_libraries(astrea-shared PUBLIC LayerShellQt::Interface)
    target_compile_definitions(astrea-shared PUBLIC ASTREA_HAVE_LAYER_SHELL_QT=1)
else()
    target_compile_definitions(astrea-shared PUBLIC ASTREA_HAVE_LAYER_SHELL_QT=0)
endif()

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

astrea_shared_enable_sanitizers(astrea-shared)

qt_add_qml_module(astrea-shared
    URI Astrea.Shared
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Astrea/Shared"
    QML_FILES
        qml/AstreaAppIcon.qml
)

option(ASTREA_SHARED_BUILD_TESTS "Build shared foundation tests" ON)
if(BUILD_TESTING AND ASTREA_SHARED_BUILD_TESTS AND ASTREA_BUILD_TESTS)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)
    add_executable(desktop-entry-catalog-test
        tests/DesktopEntryCatalogTest.cpp
        apps/DesktopEntryCatalog.cpp
    )
    target_include_directories(desktop-entry-catalog-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(desktop-entry-catalog-test PRIVATE astrea-shared Qt6::Test)
    astrea_shared_enable_sanitizers(desktop-entry-catalog-test)
    add_test(NAME desktop-entry-catalog-test COMMAND desktop-entry-catalog-test)
    if(ASTREA_ENABLE_ASAN)
        set_tests_properties(desktop-entry-catalog-test PROPERTIES
            ENVIRONMENT
                "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/AltTab/lsan.supp:print_suppressions=0;ASAN_OPTIONS=detect_leaks=1:halt_on_error=1;UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
        )
    endif()
endif()
