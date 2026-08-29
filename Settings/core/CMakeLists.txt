add_library(astrea-settings-core STATIC
    SettingsController.cpp
    SettingsController.hpp
    ../services/wallpaper/SettingsWallpaperController.cpp
    ../services/wallpaper/SettingsWallpaperController.hpp
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
    ../services/i18n/SettingsTranslationController.cpp
    ../services/i18n/SettingsTranslationController.hpp
    ../services/profile/SettingsUserProfileProvider.cpp
    ../services/profile/SettingsUserProfileProvider.hpp
    ../services/profile/SettingsUserProfile.hpp
)

target_include_directories(astrea-settings-core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(astrea-settings-core PUBLIC
    Qt6::Core Qt6::Network astrea-shared-core astrea-shared-dock)

settings_enable_warnings(astrea-settings-core)
settings_enable_sanitizers(astrea-settings-core)
