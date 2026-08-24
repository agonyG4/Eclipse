cmake_minimum_required(VERSION 3.24)
project(AstreaSettings VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(ASTREA_SETTINGS_BUILD_TESTS "Build Settings unit tests" ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Core5Compat Gui Network Qml Quick QuickControls2)

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
