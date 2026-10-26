cmake_minimum_required(VERSION 3.24)
project(AstreaSDKIndependentConsumer LANGUAGES CXX)

find_package(AstreaSDK CONFIG REQUIRED)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick)

qt_add_executable(astrea-sdk-consumer main.cpp)
target_compile_features(astrea-sdk-consumer PRIVATE cxx_std_20)
target_link_libraries(astrea-sdk-consumer PRIVATE
    AstreaSDK::Core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

enable_testing()
add_test(NAME astrea-sdk-independent-consumer COMMAND astrea-sdk-consumer)
set_tests_properties(astrea-sdk-independent-consumer PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;ASTREA_SDK_TEST_DATA_HOME=${CMAKE_BINARY_DIR}/test-data"
)
