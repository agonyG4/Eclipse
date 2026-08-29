find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

function(add_settings_unit_test target source)
    add_executable(${target} "${source}")
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        "${CMAKE_CURRENT_SOURCE_DIR}/../../shared"
    )
    target_link_libraries(${target} PRIVATE astrea-settings-core Qt6::Test)
    target_compile_definitions(${target} PRIVATE
        ASTREA_ECLIPSE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    settings_enable_warnings(${target})
    settings_enable_sanitizers(${target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_settings_unit_test(settings-controller-test unit/SettingsControllerTest.cpp)
add_settings_unit_test(settings-dock-controller-test unit/SettingsDockControllerTest.cpp)
add_settings_unit_test(settings-wallpaper-controller-test unit/SettingsWallpaperControllerTest.cpp)
add_settings_unit_test(settings-navigation-model-test unit/SettingsNavigationModelTest.cpp)
add_settings_unit_test(settings-user-profile-provider-test unit/SettingsUserProfileProviderTest.cpp)
add_settings_unit_test(admin-group-detector-test unit/AdminGroupDetectorTest.cpp)
add_settings_unit_test(administrative-group-policy-test unit/AdministrativeGroupPolicyTest.cpp)
add_settings_unit_test(theme-controller-test unit/ThemeControllerTest.cpp)

function(add_settings_qml_integration_test target source)
    qt_add_executable(${target} "${source}")
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        "${CMAKE_CURRENT_SOURCE_DIR}/../../shared"
    )
    target_link_libraries(${target} PRIVATE
        astrea-settings-core
        astrea-settings-ui
        astrea-settings-uiplugin
        astrea-shared-core
        astrea-shared-coreplugin
        Qt6::Core
        Qt6::Core5Compat
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Test
    )
    target_compile_definitions(${target} PRIVATE
        ASTREA_ECLIPSE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    qt_import_qml_plugins(${target})
    settings_enable_warnings(${target})
    settings_enable_sanitizers(${target})
    add_test(NAME ${target} COMMAND ${target})

    set(test_environment "QT_QPA_PLATFORM=offscreen")
    if(ASTREA_ENABLE_ASAN)
        # Qt's offscreen graphics teardown leaks allocations in system font and GL libraries.
        list(APPEND test_environment "ASAN_OPTIONS=detect_leaks=0")
    endif()
    set_tests_properties(${target} PROPERTIES ENVIRONMENT "${test_environment}")
endfunction()

add_settings_qml_integration_test(settings-qml-smoke-test integration/SettingsQmlSmokeTest.cpp)
add_settings_qml_integration_test(settings-component-smoke-test integration/SettingsComponentSmokeTest.cpp)

add_test(NAME compositor-page-source-test
    COMMAND ${CMAKE_COMMAND}
        -DCOMPOSITOR_QML_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/../qml/pages/system/Compositor.qml
        -P ${CMAKE_CURRENT_SOURCE_DIR}/static/CompositorPageSourceTest.cmake
)

string(JOIN "|" _settings_qml_paths ${ASTREA_SETTINGS_QML_RELATIVE_FILES})
add_test(NAME settings-structure-test
    COMMAND ${CMAKE_COMMAND}
        -DSETTINGS_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
        -DSETTINGS_QML_FILES=${_settings_qml_paths}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/static/SettingsStructureTest.cmake
)
