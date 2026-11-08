find_program(ASTREA_ICON_RUSTC rustc REQUIRED)
set(astrea_icon_policy_source "${CMAKE_CURRENT_SOURCE_DIR}/icons/icon_directory_policy.rs")
set(astrea_icon_policy_archive "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}astrea_icon_policy${CMAKE_STATIC_LIBRARY_SUFFIX}")
add_custom_command(
    OUTPUT "${astrea_icon_policy_archive}"
    COMMAND "${ASTREA_ICON_RUSTC}" --edition=2024 --crate-type=staticlib
            -C panic=abort -C relocation-model=pic -O
            -o "${astrea_icon_policy_archive}" "${astrea_icon_policy_source}"
    DEPENDS "${astrea_icon_policy_source}" "${CMAKE_CURRENT_SOURCE_DIR}/icons/icon_theme_index.rs"
    COMMENT "Compiling Rust icon directory policy"
    VERBATIM
)
add_custom_target(astrea-icon-policy-build DEPENDS "${astrea_icon_policy_archive}")
add_library(astrea-icon-policy STATIC IMPORTED GLOBAL)
set_target_properties(astrea-icon-policy PROPERTIES IMPORTED_LOCATION "${astrea_icon_policy_archive}")
add_dependencies(astrea-icon-policy astrea-icon-policy-build)

add_library(astrea-shared-core STATIC
    icons/IconDirectoryPolicy.h
    icons/IconThemeCatalog.cpp
    icons/IconThemeService.cpp
    icons/FileIconProvider.cpp
    include/AstreaSDK/IconThemeCatalog.hpp
    include/AstreaSDK/IconThemeService.hpp
    include/AstreaSDK/FileIconProvider.hpp
    settings/RegionalPreferences.cpp
    settings/RegionalPreferences.hpp
    apps/DesktopEntryParser.cpp
    apps/DesktopEntryParser.hpp
    apps/DesktopEntryCatalog.cpp
    apps/DesktopEntryCatalog.hpp
    launch/ApplicationLauncher.cpp
    launch/ApplicationLauncher.hpp
    theme/ThemeController.cpp
    theme/ThemeController.hpp
    theme/Typography.cpp
    theme/Typography.hpp
    theme/SystemFontConfig.cpp
    theme/SystemFontConfig.hpp
    theme/FontconfigDpiSync.cpp
    theme/FontconfigDpiSync.hpp
    screenshot/ScreenshotConfiguration.cpp
    screenshot/ScreenshotConfiguration.hpp
    icons/AstreaIconTheme.cpp
    icons/AstreaIconTheme.hpp
    icons/IconRenderRequest.hpp
    icons/AstreaIconProvider.cpp
    icons/AstreaIconProvider.hpp
    icons/AppTileHook.cpp
    icons/AppTileHook.hpp
)

target_include_directories(astrea-shared-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
set_target_properties(astrea-shared-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(astrea-shared-core PRIVATE ASTREA_SDK_CORE_BUILD)

if(NOT MSVC)
    target_compile_options(astrea-shared-core PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(astrea-shared-core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    PRIVATE astrea-icon-policy ${CMAKE_DL_LIBS}
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(astrea-shared-core INTERFACE
        "LINKER:--exclude-libs,${CMAKE_STATIC_LIBRARY_PREFIX}astrea_icon_policy${CMAKE_STATIC_LIBRARY_SUFFIX}")
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

function(astrea_shared_add_theme_tokens_test)
    add_executable(astrea-system-fontconfig-test tests/SystemFontConfigTest.cpp)
    target_link_libraries(astrea-system-fontconfig-test PRIVATE astrea-shared-core Qt6::Test)
    astrea_shared_enable_sanitizers(astrea-system-fontconfig-test)
    add_test(NAME astrea-system-fontconfig-test COMMAND astrea-system-fontconfig-test)

    add_executable(astrea-fontconfig-dpi-sync-test tests/FontconfigDpiSyncTest.cpp)
    target_link_libraries(astrea-fontconfig-dpi-sync-test PRIVATE astrea-shared-core Qt6::Test)
    astrea_shared_enable_sanitizers(astrea-fontconfig-dpi-sync-test)
    add_test(NAME astrea-fontconfig-dpi-sync-test COMMAND astrea-fontconfig-dpi-sync-test)
    set_tests_properties(astrea-fontconfig-dpi-sync-test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME astrea-fontconfig-dpi-sync-scaled-test COMMAND astrea-fontconfig-dpi-sync-test)
    set_tests_properties(astrea-fontconfig-dpi-sync-scaled-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=2")

    add_executable(astrea-theme-tokens-test tests/ThemeTokensTest.cpp)
    target_include_directories(astrea-theme-tokens-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(astrea-theme-tokens-test PRIVATE
        ASTREA_THEME_TOKENS_QML="${CMAKE_CURRENT_SOURCE_DIR}/qml/ThemeTokens.qml"
        ASTREA_THEME_PANEL_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../themes/examples/qml-panels/qml"
        ASTREA_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}")
    target_link_libraries(astrea-theme-tokens-test PRIVATE
        astrea-shared-core astrea-shared-coreplugin Qt6::Test Qt6::Qml Qt6::Quick)
    astrea_shared_enable_sanitizers(astrea-theme-tokens-test)
    add_test(NAME astrea-theme-tokens-test COMMAND astrea-theme-tokens-test)
    add_dependencies(astrea-theme-tokens-test astrea-shared-coreplugin)
    set_tests_properties(astrea-theme-tokens-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${CMAKE_BINARY_DIR}")
endfunction()

function(astrea_shared_add_theme_components_test)
    add_executable(astrea-theme-components-test tests/ThemeComponentsTest.cpp)
    target_include_directories(astrea-theme-components-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(astrea-theme-components-test PRIVATE astrea-shared-core Qt6::Test)
    astrea_shared_enable_sanitizers(astrea-theme-components-test)
    add_test(NAME astrea-theme-components-test COMMAND astrea-theme-components-test)
endfunction()

astrea_shared_enable_sanitizers(astrea-shared-core)

qt_add_qml_module(astrea-shared-core
    URI Astrea.Shared
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Astrea/Shared"
    QML_FILES
        qml/AstreaIcon.qml
        qml/AstreaFileIcon.qml
        qml/AstreaAppIcon.qml
        qml/ThemeTokens.qml
        qml/Typography.qml
        qml/TypographyText.qml
        qml/DisplayText.qml
        qml/TitleText.qml
        qml/BodyText.qml
        qml/LabelText.qml
        qml/CaptionText.qml
        qml/MonoText.qml
        qml/ThemeComponentLoader.qml
        qml/ShellMaterialTheme.qml
        qml/ShellCard.qml
)

# Keep the generated static QML plugin reachable through the single public SDK
# target. External applications need not know about this implementation detail.
add_library(astrea-sdk-core SHARED
    sdk/Core.cpp
    sdk/QmlPlugin.cpp
    include/AstreaSDK/Core.hpp
)
add_library(AstreaSDK::Core ALIAS astrea-sdk-core)
set_target_properties(astrea-sdk-core PROPERTIES EXPORT_NAME Core)
set_target_properties(astrea-sdk-core PROPERTIES OUTPUT_NAME AstreaSDKCore)
target_include_directories(astrea-sdk-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(astrea-sdk-core PRIVATE ASTREA_SDK_CORE_BUILD)
target_link_libraries(astrea-sdk-core
    PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    PRIVATE
    astrea-shared-core
    astrea-shared-coreplugin
)
set_target_properties(astrea-shared-coreplugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AstreaSDKConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AstreaSDKConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/AstreaSDK"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AstreaSDKConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY ExactVersion
)

install(TARGETS astrea-sdk-core
    EXPORT AstreaSDKTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT AstreaSDK
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT AstreaSDK
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/AstreaSDK"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT AstreaSDK
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AstreaSDKConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AstreaSDKConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/AstreaSDK"
    COMPONENT AstreaSDK
)
install(EXPORT AstreaSDKTargets
    FILE AstreaSDKTargets.cmake
    NAMESPACE AstreaSDK::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/AstreaSDK"
    COMPONENT AstreaSDK
)

qt_add_shaders(astrea-shared-core astrea_shared_core_shaders
    PREFIX "/shaders"
    BASE qml/shaders
    FILES
        qml/shaders/rounded_icon.frag
)

