if(NOT DEFINED ASTREA_SDK_SOURCE_DIR OR NOT DEFINED ASTREA_SDK_BUILD_DIR
   OR NOT DEFINED ASTREA_SDK_INSTALL_PREFIX
   OR NOT DEFINED ASTREA_SDK_RELOCATED_PREFIX)
    message(FATAL_ERROR "The Astrea SDK consumer test is missing a required path")
endif()

foreach(path IN ITEMS
        "${ASTREA_SDK_BUILD_DIR}"
        "${ASTREA_SDK_INSTALL_PREFIX}"
        "${ASTREA_SDK_RELOCATED_PREFIX}")
    cmake_path(IS_PREFIX ASTREA_SDK_BUILD_DIR "${path}" NORMALIZE is_in_build_dir)
    if(NOT is_in_build_dir)
        message(FATAL_ERROR "SDK test path is outside the configured build directory: ${path}")
    endif()
endforeach()

set(first_consumer "${ASTREA_SDK_BUILD_DIR}/sdk-consumer")
set(relocated_consumer "${ASTREA_SDK_BUILD_DIR}/sdk-consumer-relocated")
file(REMOVE_RECURSE
    "${ASTREA_SDK_INSTALL_PREFIX}"
    "${ASTREA_SDK_RELOCATED_PREFIX}"
    "${first_consumer}"
    "${relocated_consumer}"
)

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${ASTREA_SDK_BUILD_DIR}"
            --prefix "${ASTREA_SDK_INSTALL_PREFIX}" --component AstreaSDK
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Astrea SDK install failed (${result})\n${output}\n${error}")
endif()

function(configure_build_and_run_consumer build_dir prefix)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${ASTREA_SDK_SOURCE_DIR}" -B "${build_dir}"
                -DCMAKE_BUILD_TYPE=Release
                "-DCMAKE_PREFIX_PATH=${prefix}"
        RESULT_VARIABLE configure_result
        OUTPUT_VARIABLE configure_output
        ERROR_VARIABLE configure_error
    )
    if(NOT configure_result EQUAL 0)
        message(FATAL_ERROR
            "Independent SDK consumer configure failed (${configure_result})\n"
            "${configure_output}\n${configure_error}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" --build "${build_dir}" --config Release
                --target astrea-sdk-consumer
        RESULT_VARIABLE build_result
        OUTPUT_VARIABLE build_output
        ERROR_VARIABLE build_error
    )
    if(NOT build_result EQUAL 0)
        message(FATAL_ERROR
            "Independent SDK consumer build failed (${build_result})\n"
            "${build_output}\n${build_error}")
    endif()

    execute_process(
        COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${build_dir}" -C Release
                --output-on-failure
        RESULT_VARIABLE test_result
        OUTPUT_VARIABLE test_output
        ERROR_VARIABLE test_error
    )
    if(NOT test_result EQUAL 0)
        message(FATAL_ERROR
            "Independent SDK consumer execution failed (${test_result})\n"
            "${test_output}\n${test_error}")
    endif()
endfunction()

configure_build_and_run_consumer("${first_consumer}" "${ASTREA_SDK_INSTALL_PREFIX}")

file(RENAME "${ASTREA_SDK_INSTALL_PREFIX}" "${ASTREA_SDK_RELOCATED_PREFIX}"
     RESULT rename_result)
if(rename_result)
    message(FATAL_ERROR "Could not relocate the installed SDK: ${rename_result}")
endif()

configure_build_and_run_consumer("${relocated_consumer}"
                                 "${ASTREA_SDK_RELOCATED_PREFIX}")
