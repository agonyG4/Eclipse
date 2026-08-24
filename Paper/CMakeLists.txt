cmake_minimum_required(VERSION 3.24)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick Test)

add_library(astrea-paper-core STATIC
    core/WallpaperCatalog.cpp
    core/WallpaperCatalog.hpp
    core/WallpaperDescriptor.cpp
    core/WallpaperDescriptor.hpp
    core/WallpaperResolver.cpp
    core/WallpaperResolver.hpp
    core/WallpaperPersistence.cpp
    core/WallpaperPersistence.hpp
    core/WallpaperService.cpp
    core/WallpaperService.hpp
    core/WallpaperSourceWatcher.cpp
    core/WallpaperSourceWatcher.hpp
    core/WallpaperValidationWorker.cpp
    core/WallpaperValidationWorker.hpp
    platform/wayland/WallpaperSurfaceManager.cpp
    platform/wayland/WallpaperSurfaceManager.hpp
    platform/wayland/WallpaperSurfaceBundle.cpp
    platform/wayland/WallpaperSurfaceBundle.hpp
    platform/wayland/WallpaperSurfacePolicy.cpp
    platform/wayland/WallpaperSurfacePolicy.hpp
    platform/ipc/WallpaperControlServer.cpp
    platform/ipc/WallpaperControlServer.hpp
)
target_include_directories(astrea-paper-core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_link_libraries(astrea-paper-core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    astrea-shared-layer-shell
)

target_compile_definitions(astrea-paper-core PRIVATE
    ASTREA_PAPER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

install(FILES assets/default.jpg
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AstreaOS/wallpapers"
)

if(NOT MSVC)
    target_compile_options(astrea-paper-core PRIVATE -Wall -Wextra -Wpedantic)
endif()

qt_add_qml_module(astrea-paper-core
    URI Astrea.Paper
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Astrea/Paper"
    QML_FILES
        qml/WallpaperSurface.qml
    RESOURCES
        assets/default.jpg
        assets/emergency.svg
)

if(BUILD_TESTING AND ASTREA_BUILD_TESTS)
    add_executable(paper-catalog-test
        tests/WallpaperCatalogTest.cpp
    )
    target_include_directories(paper-catalog-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_link_libraries(paper-catalog-test PRIVATE
        astrea-paper-core
        Qt6::Core
        Qt6::Gui
        Qt6::Test
    )
    add_test(NAME paper-catalog-test COMMAND paper-catalog-test)

    add_executable(paper-descriptor-test
        tests/WallpaperDescriptorTest.cpp
    )
    target_include_directories(paper-descriptor-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_link_libraries(paper-descriptor-test PRIVATE
        astrea-paper-core
        Qt6::Core
        Qt6::Gui
        Qt6::Test
    )
    add_test(NAME paper-descriptor-test COMMAND paper-descriptor-test)

    add_executable(paper-resolver-test
        tests/WallpaperResolverTest.cpp
    )
    target_include_directories(paper-resolver-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_link_libraries(paper-resolver-test PRIVATE
        astrea-paper-core
        Qt6::Core
        Qt6::Gui
        Qt6::Test
    )
    add_test(NAME paper-resolver-test COMMAND paper-resolver-test)

    add_executable(paper-persistence-test
        tests/WallpaperPersistenceTest.cpp
    )
    target_include_directories(paper-persistence-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_link_libraries(paper-persistence-test PRIVATE
        astrea-paper-core
        Qt6::Core
        Qt6::Gui
        Qt6::Test
    )
    add_test(NAME paper-persistence-test COMMAND paper-persistence-test)

    add_executable(paper-service-test
        tests/WallpaperServiceTest.cpp
    )
    target_include_directories(paper-service-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_link_libraries(paper-service-test PRIVATE
        astrea-paper-core
        Qt6::Core
        Qt6::Gui
        Qt6::Test
    )
    add_test(NAME paper-service-test COMMAND paper-service-test)

    add_executable(paper-surface-manager-test
        tests/WallpaperSurfaceManagerTest.cpp
    )
    target_include_directories(paper-surface-manager-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_link_libraries(paper-surface-manager-test PRIVATE
        astrea-paper-core
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
    )
    qt_import_qml_plugins(paper-surface-manager-test)
    add_test(NAME paper-surface-manager-test COMMAND paper-surface-manager-test)
    set_tests_properties(paper-surface-manager-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(paper-surface-policy-test
        tests/WallpaperSurfacePolicyTest.cpp
    )
    target_include_directories(paper-surface-policy-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_link_libraries(paper-surface-policy-test PRIVATE
        astrea-paper-core
        Qt6::Core
        Qt6::Gui
        Qt6::Test
    )
    add_test(NAME paper-surface-policy-test COMMAND paper-surface-policy-test)
    set_tests_properties(paper-surface-policy-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(paper-control-server-test
        tests/WallpaperControlServerTest.cpp
    )
    target_include_directories(paper-control-server-test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
    target_link_libraries(paper-control-server-test PRIVATE
        astrea-paper-core
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Test
    )
    add_test(NAME paper-control-server-test COMMAND paper-control-server-test)
endif()
