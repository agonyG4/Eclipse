include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/AstreaCxxQtBackend.cmake")

# Keep the Settings crate on the established 0.10 CXX-Qt boundary. The shared
# resolver retains find_package(CxxQt QUIET), LOCKED cargo imports, and checks
# both Qt6Core_DIR and qmake's QT_INSTALL_PREFIX against the Qt selected by CMake.
astrea_add_cxx_qt_crate(
    NAME ASTREA_SETTINGS
    MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/backend/Cargo.toml"
    EXPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/cxxqt"
    CRATE_NAME astrea_settings_backend
    QMAKE_VARIABLE ASTREA_SETTINGS_QMAKE
    QT_MODULES Qt6::Core Qt6::Network
    CXXQT_HEADERS
        src/animation/qobject.cxxqt.h
        src/appearance/qobject.cxxqt.h
        src/icons/qobject.cxxqt.h
)
