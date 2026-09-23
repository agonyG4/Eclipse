set(ASTREA_SETTINGS_BACKEND_CXXQT_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/../cxxqt/crates/astrea_settings_backend/include/astrea_settings_backend/src/animation/qobject.cxxqt.h"
    "${CMAKE_CURRENT_BINARY_DIR}/../cxxqt/crates/astrea_settings_backend/include/astrea_settings_backend/src/appearance/qobject.cxxqt.h"
    "${CMAKE_CURRENT_BINARY_DIR}/../cxxqt/crates/astrea_settings_backend/include/astrea_settings_backend/src/icons/qobject.cxxqt.h"
)
set_source_files_properties(${ASTREA_SETTINGS_BACKEND_CXXQT_HEADERS}
    PROPERTIES GENERATED TRUE
)

add_library(astrea-settings-core STATIC
    SettingsController.cpp
    SettingsController.hpp
    ${ASTREA_SETTINGS_BACKEND_CXXQT_HEADERS}
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
    ../services/dock/SettingsDockController.cpp
    ../services/dock/SettingsDockController.hpp
    ../services/i18n/SettingsTranslationController.cpp
    ../services/i18n/SettingsTranslationController.hpp
    ../services/profile/SettingsUserProfileProvider.cpp
    ../services/profile/SettingsUserProfileProvider.hpp
    ../services/profile/SettingsUserProfile.hpp
)
add_dependencies(astrea-settings-core astrea_settings_backend_cxxqt_headers)

target_include_directories(astrea-settings-core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_include_directories(astrea-settings-core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../shared"
)

target_link_libraries(astrea-settings-core PUBLIC
    Qt6::Core
    astrea-shared-dock
    astrea-shared-system
    astrea_settings_backend
)
target_link_libraries(astrea-settings-core PRIVATE
    Qt6::Network
)

# The Settings and shared-system CXX-Qt static libraries each contain the
# version-pinned bridge support symbols. Resolve their identical ABI copies
# when a Settings executable consumes both backends.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND UNIX AND NOT APPLE)
    target_link_options(astrea-settings-core INTERFACE
        "LINKER:--allow-multiple-definition"
    )
endif()

settings_enable_warnings(astrea-settings-core)
settings_enable_sanitizers(astrea-settings-core)
