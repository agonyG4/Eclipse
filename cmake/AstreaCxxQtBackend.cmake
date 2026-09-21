include_guard(GLOBAL)

find_package(CxxQt QUIET)
if(NOT CxxQt_FOUND)
    include(FetchContent)
    FetchContent_Declare(cxx_qt_cmake
        GIT_REPOSITORY https://github.com/KDAB/cxx-qt-cmake.git
        GIT_TAG 0.10.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(cxx_qt_cmake)
    include("${cxx_qt_cmake_SOURCE_DIR}/cmake/CxxQt.cmake")
endif()

if(NOT COMMAND cxx_qt_import_crate)
    message(FATAL_ERROR "CXX-Qt 0.10.0 CMake integration is unavailable")
endif()

function(astrea_add_cxx_qt_crate)
    cmake_parse_arguments(PARSE_ARGV 0 args ""
        "NAME;MANIFEST_PATH;CRATE_NAME;EXPORT_DIR;QMAKE_VARIABLE"
        "QT_MODULES;CXXQT_HEADERS")
    foreach(required IN ITEMS NAME MANIFEST_PATH CRATE_NAME EXPORT_DIR QMAKE_VARIABLE)
        if(NOT args_${required})
            message(FATAL_ERROR "astrea_add_cxx_qt_crate requires ${required}")
        endif()
    endforeach()
    if(NOT args_QT_MODULES)
        message(FATAL_ERROR "astrea_add_cxx_qt_crate requires at least one Qt module")
    endif()

    if(NOT DEFINED Qt6_DIR AND NOT DEFINED Qt6Core_DIR)
        message(FATAL_ERROR "Cannot derive qmake because the selected Qt6 package directory is unavailable")
    endif()
    set(_astrea_qt_prefix_candidates)
    foreach(_astrea_qt_cmake_dir IN ITEMS "${Qt6_DIR}" "${Qt6Core_DIR}")
        if(_astrea_qt_cmake_dir)
            get_filename_component(_astrea_candidate "${_astrea_qt_cmake_dir}/../../" REALPATH)
            list(APPEND _astrea_qt_prefix_candidates "${_astrea_candidate}")
            get_filename_component(_astrea_candidate "${_astrea_qt_cmake_dir}/../../../" REALPATH)
            list(APPEND _astrea_qt_prefix_candidates "${_astrea_candidate}")
            get_filename_component(_astrea_candidate "${_astrea_qt_cmake_dir}/../../../../" REALPATH)
            list(APPEND _astrea_qt_prefix_candidates "${_astrea_candidate}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _astrea_qt_prefix_candidates)

    set(_astrea_selected_qmake "")
    set(_astrea_selected_qt_prefix "")
    if(TARGET Qt6::qmake)
        get_target_property(_astrea_selected_qmake Qt6::qmake IMPORTED_LOCATION_RELEASE)
        if(NOT _astrea_selected_qmake OR _astrea_selected_qmake MATCHES "-NOTFOUND$")
            get_target_property(_astrea_selected_qmake Qt6::qmake IMPORTED_LOCATION)
        endif()
    endif()

    if(NOT _astrea_selected_qmake OR _astrea_selected_qmake MATCHES "-NOTFOUND$")
        foreach(_astrea_qt_prefix IN LISTS _astrea_qt_prefix_candidates)
            foreach(_astrea_qmake_name IN ITEMS qmake6 qmake)
                if(EXISTS "${_astrea_qt_prefix}/bin/${_astrea_qmake_name}")
                    set(_astrea_selected_qmake "${_astrea_qt_prefix}/bin/${_astrea_qmake_name}")
                    set(_astrea_selected_qt_prefix "${_astrea_qt_prefix}")
                    break()
                endif()
                if(EXISTS "${_astrea_qt_prefix}/libexec/${_astrea_qmake_name}")
                    set(_astrea_selected_qmake "${_astrea_qt_prefix}/libexec/${_astrea_qmake_name}")
                    set(_astrea_selected_qt_prefix "${_astrea_qt_prefix}")
                    break()
                endif()
            endforeach()
            if(_astrea_selected_qmake)
                break()
            endif()
        endforeach()
    endif()

    if(NOT _astrea_selected_qmake OR NOT EXISTS "${_astrea_selected_qmake}")
        message(FATAL_ERROR "Could not resolve qmake from the Qt6 installation selected by CMake")
    endif()

    execute_process(
        COMMAND "${_astrea_selected_qmake}" -query QT_VERSION
        RESULT_VARIABLE _astrea_qmake_version_result
        OUTPUT_VARIABLE _astrea_qmake_version
        ERROR_VARIABLE _astrea_qmake_version_error
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _astrea_qmake_version_result EQUAL 0)
        message(FATAL_ERROR "The qmake selected by CMake could not report QT_VERSION: ${_astrea_qmake_version_error}")
    endif()
    if(_astrea_qmake_version VERSION_LESS 6.8 OR
       (DEFINED Qt6Core_VERSION AND NOT _astrea_qmake_version VERSION_EQUAL Qt6Core_VERSION))
        message(FATAL_ERROR
            "CMake selected Qt ${Qt6Core_VERSION}, but qmake reports Qt ${_astrea_qmake_version}")
    endif()

    execute_process(
        COMMAND "${_astrea_selected_qmake}" -query QT_INSTALL_PREFIX
        RESULT_VARIABLE _astrea_qmake_prefix_result
        OUTPUT_VARIABLE _astrea_qmake_prefix
        ERROR_VARIABLE _astrea_qmake_prefix_error
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _astrea_qmake_prefix_result EQUAL 0)
        message(FATAL_ERROR "The qmake selected by CMake could not report QT_INSTALL_PREFIX: ${_astrea_qmake_prefix_error}")
    endif()
    get_filename_component(_astrea_qmake_prefix "${_astrea_qmake_prefix}" REALPATH)
    if(NOT _astrea_selected_qt_prefix)
        list(FIND _astrea_qt_prefix_candidates "${_astrea_qmake_prefix}" _astrea_prefix_index)
        if(_astrea_prefix_index EQUAL -1)
            message(FATAL_ERROR "qmake is not associated with the Qt6 installation selected by CMake")
        endif()
        set(_astrea_selected_qt_prefix "${_astrea_qmake_prefix}")
    endif()
    if(NOT _astrea_qmake_prefix STREQUAL _astrea_selected_qt_prefix)
        message(FATAL_ERROR
            "Qt installation mismatch: CMake selected '${_astrea_selected_qt_prefix}', qmake belongs to '${_astrea_qmake_prefix}'")
    endif()

    set(${args_QMAKE_VARIABLE} "${_astrea_selected_qmake}" PARENT_SCOPE)
    cxx_qt_import_crate(
        MANIFEST_PATH "${args_MANIFEST_PATH}"
        CXX_QT_EXPORT_DIR "${args_EXPORT_DIR}"
        LOCKED
        CRATES "${args_CRATE_NAME}"
        QMAKE "${_astrea_selected_qmake}"
        QT_MODULES ${args_QT_MODULES}
    )

    if(args_CXXQT_HEADERS)
        set(_astrea_cxxqt_header_outputs)
        set(_astrea_cxxqt_rust_sources)
        get_filename_component(_astrea_manifest_dir "${args_MANIFEST_PATH}" DIRECTORY)
        foreach(_astrea_header IN LISTS args_CXXQT_HEADERS)
            list(APPEND _astrea_cxxqt_header_outputs
                "${args_EXPORT_DIR}/crates/${args_CRATE_NAME}/include/${args_CRATE_NAME}/${_astrea_header}")
            string(REGEX REPLACE "\\.cxxqt\\.h$" ".rs" _astrea_rust_source "${_astrea_header}")
            if(EXISTS "${_astrea_manifest_dir}/${_astrea_rust_source}")
                list(APPEND _astrea_cxxqt_rust_sources
                    "${_astrea_manifest_dir}/${_astrea_rust_source}")
            endif()
        endforeach()

        # Qt's AUTOMOC tracks these generated QObject headers as inputs. Give
        # Ninja an explicit producer so it builds the CXX-Qt crate before
        # checking those header dependencies on a clean build.
        add_custom_command(
            OUTPUT ${_astrea_cxxqt_header_outputs}
            COMMAND "${CMAKE_COMMAND}" -E true
            DEPENDS "${args_CRATE_NAME}_mock_initializers" ${_astrea_cxxqt_rust_sources}
            COMMENT "Waiting for ${args_CRATE_NAME} CXX-Qt headers"
            VERBATIM
        )
        add_custom_target(${args_CRATE_NAME}_cxxqt_headers
            DEPENDS ${_astrea_cxxqt_header_outputs})
    endif()
endfunction()
