cmake_minimum_required(VERSION 3.16)
project(AstreaShared LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)

add_library(astrea-shared STATIC
    icons/AstreaIconTheme.cpp
    icons/AstreaIconTheme.hpp
    icons/AstreaIconProvider.cpp
    icons/AstreaIconProvider.hpp
)

target_include_directories(astrea-shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(astrea-shared PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)
