cmake_minimum_required(VERSION 3.24)
project(AstreaAltTab VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ASTREA_ALT_TAB_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick Network)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/AltTabWarnings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/AltTabSanitizers.cmake)

# Link shared libraries sibling folder
if(NOT TARGET astrea-shared-core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_CURRENT_BINARY_DIR}/shared)
endif()

# 1. Types Library (WindowInfo, CompositorBackend interface, CompositorTypes)
#    This is the base layer — no dependencies on other local libs.
add_library(astrea-alttab-types STATIC
    core/WindowInfo.cpp
    platform/compositor/CompositorBackend.cpp
)
target_include_directories(astrea-alttab-types PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(astrea-alttab-types PUBLIC Qt6::Core Qt6::Gui)
astrea_enable_warnings(astrea-alttab-types)
astrea_enable_sanitizers(astrea-alttab-types)

# 2. Identity Library
add_library(astrea-alttab-identity STATIC
    services/AppIdentityResolver.cpp
    services/appidentity/ProcessInspector.cpp
    services/appidentity/SteamMetadataIndex.cpp
    services/appidentity/WineExecutableResolver.cpp
)
target_include_directories(astrea-alttab-identity PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(astrea-alttab-identity PUBLIC Qt6::Core Qt6::Gui astrea-shared-core astrea-alttab-types)
astrea_enable_warnings(astrea-alttab-identity)
astrea_enable_sanitizers(astrea-alttab-identity)

# 3. Hyprland Backend Library
add_library(astrea-alttab-hyprland STATIC
    platform/hyprland/HyprlandCommand.cpp
    platform/hyprland/HyprlandWindowSource.cpp
    platform/hyprland/HyprlandSocket.cpp
)
target_include_directories(astrea-alttab-hyprland PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(astrea-alttab-hyprland PUBLIC Qt6::Core Qt6::Network astrea-alttab-identity)
astrea_enable_warnings(astrea-alttab-hyprland)
astrea_enable_sanitizers(astrea-alttab-hyprland)

# 4. Core Library
add_library(astrea-alttab-core STATIC
    core/AltTabController.cpp
    core/AltTabWindowModel.cpp
    platform/compositor/CompositorBackendFactory.cpp
    platform/wayland/LayerShellSurface.cpp
    platform/runtime/AltTabRuntimePaths.cpp
    services/AltTabConfigWatcher.cpp
    platform/ipc/AltTabIpcServer.cpp
)
target_include_directories(astrea-alttab-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(astrea-alttab-core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network
    astrea-alttab-types
    astrea-alttab-identity
    astrea-alttab-hyprland
    astrea-shared-layer-shell
)
astrea_enable_warnings(astrea-alttab-core)
astrea_enable_sanitizers(astrea-alttab-core)

# Main Executable
qt_add_executable(astrea-alt-tab
    app/main.cpp
    app/CommandLine.cpp
    app/AltTabApplication.cpp
)
target_include_directories(astrea-alt-tab PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(astrea-alt-tab PRIVATE
    astrea-alttab-core
    astrea-shared-coreplugin
    dl pthread
)

qt_add_qml_module(astrea-alt-tab
    URI Astrea.AltTab
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/components/AltTabPanel.qml
        qml/components/AltTabWindowDelegate.qml
        qml/components/AstreaAppIcon.qml
)
qt_import_qml_plugins(astrea-alt-tab)

astrea_enable_warnings(astrea-alt-tab)
astrea_enable_sanitizers(astrea-alt-tab)

install(TARGETS astrea-alt-tab RUNTIME DESTINATION bin)
install(FILES packaging/systemd/astrea-alt-tabd.service DESTINATION share/systemd/user)
install(FILES packaging/hypr/astrea-alt-tab.conf DESTINATION share/hypr)

include(CTest)
if(BUILD_TESTING AND ASTREA_ALT_TAB_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

    set(ALTTAB_TEST_TARGETS
        alttab-controller-test
        alttab-ipc-test
        alttab-identity-test
        alttab-source-test
        alttab-socket-test
        alttab-model-test
    )

    function(add_alttab_test TEST_NAME TEST_FILE)
        add_executable(${TEST_NAME}
            ${TEST_FILE}
            ${ARGN}
        )
        target_include_directories(${TEST_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
        target_link_libraries(${TEST_NAME} PRIVATE
            astrea-alttab-core
            Qt6::Test
        )
        astrea_enable_sanitizers(${TEST_NAME})
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endfunction()

    add_alttab_test(alttab-controller-test tests/AltTabControllerTest.cpp platform/compositor/FakeBackend.cpp)
    add_alttab_test(alttab-ipc-test tests/AltTabIpcTest.cpp)
    add_alttab_test(alttab-identity-test tests/AppIdentityResolverTest.cpp)
    add_alttab_test(alttab-source-test tests/HyprlandWindowSourceTest.cpp)
    add_alttab_test(alttab-socket-test tests/HyprlandSocketTest.cpp)
    add_alttab_test(alttab-model-test tests/AltTabWindowModelTest.cpp)

    # Sanitizer test environment with LSAN suppression for NVIDIA driver
    set(_lsan_supp "${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp")
    if(EXISTS "${_lsan_supp}")
        set(_lsan_options "suppressions=${_lsan_supp}:print_suppressions=0")
        set_tests_properties(${ALTTAB_TEST_TARGETS} PROPERTIES
            ENVIRONMENT
                "LSAN_OPTIONS=${_lsan_options};ASAN_OPTIONS=detect_leaks=1:halt_on_error=1;UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
        )
    endif()
endif()
