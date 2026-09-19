cmake_minimum_required(VERSION 3.24)
project(AstreaSettings VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ASTREA_SETTINGS_BUILD_TESTS "Build Settings unit tests" ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Core5Compat Gui Network Qml Quick QuickControls2)

include(FetchContent)
FetchContent_Declare(cxx_qt_cmake
    GIT_REPOSITORY https://github.com/KDAB/cxx-qt-cmake.git
    GIT_TAG 0.10.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(cxx_qt_cmake)
include("${cxx_qt_cmake_SOURCE_DIR}/cmake/CxxQt.cmake")

find_program(ASTREA_SETTINGS_QMAKE NAMES qmake6 qmake REQUIRED)
cxx_qt_import_crate(
    MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/backend/Cargo.toml"
    CXX_QT_EXPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/cxxqt"
    QMAKE "${ASTREA_SETTINGS_QMAKE}"
    QT_MODULES Qt6::Core Qt6::Network
)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SettingsWarnings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SettingsSanitizers.cmake)

if(NOT TARGET astrea-shared-core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_CURRENT_BINARY_DIR}/shared)
endif()

add_subdirectory(core)
add_subdirectory(qml)
add_subdirectory(app)

if(BUILD_TESTING AND ASTREA_SETTINGS_BUILD_TESTS AND ASTREA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
