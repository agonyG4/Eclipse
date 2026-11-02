add_library(astrea-shared-core STATIC
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
    screenshot/ScreenshotConfiguration.cpp
    screenshot/ScreenshotConfiguration.hpp
    icons/AstreaIconTheme.cpp
    icons/AstreaIconTheme.hpp
    icons/IconRenderRequest.hpp
    icons/AstreaIconProvider.cpp
    icons/AstreaIconProvider.hpp
)

target_include_directories(astrea-shared-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
set_target_properties(astrea-shared-core PROPERTIES POSITION_INDEPENDENT_CODE ON)

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

function(astrea_shared_add_theme_tokens_test)
    add_executable(astrea-system-fontconfig-test tests/SystemFontConfigTest.cpp)
    target_link_libraries(astrea-system-fontconfig-test PRIVATE astrea-shared-core Qt6::Test)
    astrea_shared_enable_sanitizers(astrea-system-fontconfig-test)
    add_test(NAME astrea-system-fontconfig-test COMMAND astrea-system-fontconfig-test)

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

