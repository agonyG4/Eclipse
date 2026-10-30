if(NOT DEFINED ASTREA_SDK_SOURCE_DIR OR NOT DEFINED ASTREA_SDK_PARENT_BINARY_DIR
   OR NOT DEFINED ASTREA_SDK_PROBE_DIR)
    message(FATAL_ERROR "The SDK-only configure test is missing a required path")
endif()

cmake_path(IS_PREFIX ASTREA_SDK_PARENT_BINARY_DIR "${ASTREA_SDK_PROBE_DIR}" NORMALIZE is_in_build_dir)
if(NOT is_in_build_dir)
    message(FATAL_ERROR "SDK-only configure probe is outside the build directory: ${ASTREA_SDK_PROBE_DIR}")
endif()

file(REMOVE_RECURSE "${ASTREA_SDK_PROBE_DIR}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${ASTREA_SDK_SOURCE_DIR}" -B "${ASTREA_SDK_PROBE_DIR}"
        -G Ninja
        -DASTREA_BUILD_SDK_ONLY=ON
        -DASTREA_BUILD_TESTS=OFF
        -DBUILD_TESTING=OFF
        -DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=TRUE
        -DCMAKE_DISABLE_FIND_PACKAGE_LayerShellQt=TRUE
        -DCMAKE_DISABLE_FIND_PACKAGE_CxxQt=TRUE
        -DFETCHCONTENT_FULLY_DISCONNECTED=TRUE
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "SDK-only configure failed while backend package discovery was disabled (${configure_result})\n"
        "${configure_output}\n${configure_error}")
endif()

if(NOT EXISTS "${ASTREA_SDK_PROBE_DIR}/CMakeCache.txt")
    message(FATAL_ERROR "SDK-only configure did not produce a CMake cache")
endif()
