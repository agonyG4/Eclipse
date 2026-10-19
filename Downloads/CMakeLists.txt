find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)

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
target_link_libraries(astrea-downloads PRIVATE astrea-downloads-api)
target_compile_options(astrea-downloads PRIVATE -Wall -Wextra -Wpedantic)
install(TARGETS astrea-downloads RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

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
    set_tests_properties(download-progress-test download-cli-integration-test
        PROPERTIES TIMEOUT 45)
endif()
