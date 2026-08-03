cmake_minimum_required(VERSION 3.24)
project(Eclipse VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASTREA_BUILD_TESTS "Build Eclipse component tests" ON)
option(ASTREA_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ASTREA_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ASTREA_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(ASTREA_ENABLE_SANITIZERS)
    set(ASTREA_ENABLE_ASAN ON CACHE BOOL "Enable AddressSanitizer" FORCE)
    set(ASTREA_ENABLE_UBSAN ON CACHE BOOL "Enable UndefinedBehaviorSanitizer" FORCE)
endif()

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Qml)
if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

include(CTest)

if(NOT TARGET astrea-shared-core)
    add_subdirectory(shared)
endif()
add_subdirectory(Spotlight)
add_subdirectory(AltTab)
add_subdirectory(Dock)
add_subdirectory(Settings)

if(BUILD_TESTING)
    add_test(NAME create-source-archive-test
        COMMAND bash ${CMAKE_SOURCE_DIR}/tools/create-source-archive-test.sh
                ${CMAKE_SOURCE_DIR}/tools/create-source-archive)
endif()
