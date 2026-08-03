qt_add_executable(astrea-settings
    main.cpp
    SettingsApplication.cpp
    SettingsApplication.hpp
)

target_include_directories(astrea-settings PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../../shared"
)

target_link_libraries(astrea-settings PRIVATE
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
)

qt_import_qml_plugins(astrea-settings)

settings_enable_warnings(astrea-settings)
settings_enable_sanitizers(astrea-settings)

install(TARGETS astrea-settings RUNTIME DESTINATION bin)
install(FILES ../packaging/applications/astrea-settings.desktop
    DESTINATION share/applications)
