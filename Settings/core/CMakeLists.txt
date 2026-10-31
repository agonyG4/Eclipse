set(ASTREA_SETTINGS_BACKEND_CXXQT_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/../cxxqt/crates/astrea_settings_backend/include/astrea_settings_backend/src/animation/qobject.cxxqt.h"
    "${CMAKE_CURRENT_BINARY_DIR}/../cxxqt/crates/astrea_settings_backend/include/astrea_settings_backend/src/appearance/qobject.cxxqt.h"
    "${CMAKE_CURRENT_BINARY_DIR}/../cxxqt/crates/astrea_settings_backend/include/astrea_settings_backend/src/icons/qobject.cxxqt.h"
    "${CMAKE_CURRENT_BINARY_DIR}/../cxxqt/crates/astrea_settings_backend/include/astrea_settings_backend/src/display/qobject.cxxqt.h"
    "${CMAKE_CURRENT_BINARY_DIR}/../cxxqt/crates/astrea_settings_backend/include/astrea_settings_backend/src/gaming/qobject.cxxqt.h"
    "${CMAKE_CURRENT_BINARY_DIR}/../cxxqt/crates/astrea_settings_backend/include/astrea_settings_backend/src/performance/qobject.cxxqt.h"
)
set_source_files_properties(${ASTREA_SETTINGS_BACKEND_CXXQT_HEADERS}
    PROPERTIES GENERATED TRUE
)

add_library(astrea-settings-core STATIC
    ../services/desktop/NativeSettingsSupport.cpp
    ../services/desktop/NativeSettingsSupport.hpp
    ../services/desktop/RegionSettings.cpp
    ../services/desktop/RegionSettings.hpp
    ../services/desktop/ApplicationsBackend.cpp
    ../services/desktop/ApplicationsBackend.hpp
    ../services/desktop/ProfileBackend.cpp
    ../services/desktop/ProfileBackend.hpp
    ../services/devices/SettingsDeviceInventory.cpp
    ../services/devices/SettingsDeviceInventory.hpp
    ../services/desktop/DesktopControllers.cpp
    ../services/desktop/DesktopControllers.hpp
    SettingsController.cpp
    SettingsController.hpp
    ${ASTREA_SETTINGS_BACKEND_CXXQT_HEADERS}
    ../services/wallpaper/SettingsWallpaperController.cpp
    ../services/wallpaper/SettingsWallpaperController.hpp
    ../services/audio/SettingsSpatialAudioController.cpp
    ../services/audio/SettingsSpatialAudioController.hpp
    navigation/SettingsNavigationCatalog.cpp
    navigation/SettingsNavigationCatalog.hpp
    navigation/SettingsNavigationEntry.hpp
    navigation/SettingsNavigationModel.cpp
    navigation/SettingsNavigationModel.hpp
    ../platform/linux/AdminGroupDetector.cpp
    ../platform/linux/AdminGroupDetector.hpp
    ../platform/linux/AdministrativeGroupPolicy.cpp
    ../platform/linux/AdministrativeGroupPolicy.hpp
    ../services/assets/SettingsIconResolver.cpp
    ../services/assets/SettingsIconResolver.hpp
    ../services/dock/SettingsDockController.cpp
    ../services/dock/SettingsDockController.hpp
    ../services/network/SettingsFirewallStatus.cpp
    ../services/network/SettingsFirewallStatus.hpp
    ../services/network/SettingsFirewallManager.cpp
    ../services/network/SettingsFirewallManager.hpp
    ../services/i18n/SettingsTranslationController.cpp
    ../services/i18n/SettingsTranslationController.hpp
    ../services/profile/SettingsUserProfileProvider.cpp
    ../services/profile/SettingsUserProfileProvider.hpp
    ../services/profile/SettingsUserProfile.hpp
)
add_dependencies(astrea-settings-core astrea_settings_backend_cxxqt_headers)

target_include_directories(astrea-settings-core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../../shared"
)

target_link_libraries(astrea-settings-core PUBLIC
    Qt6::Core
    astrea-shared-core
    astrea-shared-dock
    astrea-shared-system-qt
    astrea_settings_backend
)
target_link_libraries(astrea-settings-core PRIVATE
    Qt6::Network
    Qt6::Concurrent
    Qt6::DBus
)

settings_enable_warnings(astrea-settings-core)
settings_enable_sanitizers(astrea-settings-core)

include(GNUInstallDirs)
set(ASTREA_PROFILE_HELPER_PATH "${CMAKE_INSTALL_FULL_LIBEXECDIR}/astrea-settings-profile-helper")
add_library(astrea-settings-profile-platform STATIC
    ../services/desktop/SddmAutologin.cpp ../services/desktop/SddmAutologin.hpp)
target_include_directories(astrea-settings-profile-platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(astrea-settings-profile-platform PUBLIC Qt6::Core)
target_link_libraries(astrea-settings-core PRIVATE astrea-settings-profile-platform)
target_compile_definitions(astrea-settings-core PRIVATE
    ASTREA_PROFILE_HELPER_PATH="${ASTREA_PROFILE_HELPER_PATH}")
add_executable(astrea-settings-profile-helper ../platform/linux/SettingsProfileHelper.cpp)
target_link_libraries(astrea-settings-profile-helper PRIVATE astrea-settings-profile-platform)
install(TARGETS astrea-settings-profile-helper RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")
configure_file(../packaging/org.astrea.settings.profile.policy.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.astrea.settings.profile.policy" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.astrea.settings.profile.policy"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/polkit-1/actions")
