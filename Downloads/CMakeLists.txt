find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(_download_browser_outputs
    "${CMAKE_CURRENT_BINARY_DIR}/astrea-downloads-native-host"
    "${CMAKE_CURRENT_BINARY_DIR}/browser/zen/manifest.json"
    "${CMAKE_CURRENT_BINARY_DIR}/browser/brave/manifest.json"
    "${CMAKE_CURRENT_BINARY_DIR}/browser/zen/background.js"
    "${CMAKE_CURRENT_BINARY_DIR}/browser/brave/background.js"
    "${CMAKE_CURRENT_BINARY_DIR}/browser/zen/config.js"
    "${CMAKE_CURRENT_BINARY_DIR}/browser/brave/config.js"
    "${CMAKE_CURRENT_BINARY_DIR}/browser/brave/worker.js")
add_custom_command(OUTPUT ${_download_browser_outputs}
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare-browser-bridge.py"
            --output "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS tools/prepare-browser-bridge.py browser/background.js browser/native_host.py
            browser/zen.json browser/brave.json
    VERBATIM)
add_custom_target(astrea-downloads-browser-assets DEPENDS ${_download_browser_outputs})

add_library(astrea-downloads-api STATIC
    core/DownloadStore.cpp
    platform/DownloadServer.cpp
    platform/DownloadServer.hpp
)
set_target_properties(astrea-downloads-api PROPERTIES AUTOMOC ON)
target_include_directories(astrea-downloads-api PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(astrea-downloads-api PUBLIC Qt6::Core Qt6::Network)
target_compile_options(astrea-downloads-api PRIVATE -Wall -Wextra -Wpedantic)

add_executable(astrea-downloads app/main.cpp)
add_dependencies(astrea-downloads astrea-downloads-browser-assets)
target_link_libraries(astrea-downloads PRIVATE astrea-downloads-api)
target_compile_options(astrea-downloads PRIVATE -Wall -Wextra -Wpedantic)
install(TARGETS astrea-downloads RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS browser/native_host.py DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME astrea-downloads-native-host)
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/browser/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/astrea/downloads/browser")

if(BUILD_TESTING AND ASTREA_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_executable(download-progress-test tests/DownloadProgressTest.cpp)
    set_target_properties(download-progress-test PROPERTIES AUTOMOC ON)
    target_link_libraries(download-progress-test PRIVATE astrea-downloads-api Qt6::Test)
    add_test(NAME download-progress-test COMMAND download-progress-test)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME download-cli-integration-test
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_integration.py
                $<TARGET_FILE:astrea-downloads>)
    add_test(NAME download-native-bridge-test
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/native_bridge_integration.py
                "${CMAKE_CURRENT_BINARY_DIR}/astrea-downloads-native-host"
                $<TARGET_FILE:astrea-downloads>)
    add_test(NAME download-browser-packaging-test
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/browser_packaging_test.py
                "${CMAKE_CURRENT_SOURCE_DIR}/tools" $<TARGET_FILE:astrea-downloads>)
    find_program(ASTREA_DOWNLOADS_NODE node)
    if(ASTREA_DOWNLOADS_NODE)
        add_test(NAME download-browser-adapter-test
            COMMAND ${ASTREA_DOWNLOADS_NODE} --test
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/browser_bridge.test.cjs")
    endif()
    set_tests_properties(download-progress-test download-cli-integration-test
        download-native-bridge-test
        download-browser-packaging-test
        PROPERTIES TIMEOUT 45)
endif()
