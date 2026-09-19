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

set(ASTREA_SETTINGS_QMAKE "")
if(TARGET Qt6::qmake)
    get_target_property(ASTREA_SETTINGS_QMAKE Qt6::qmake IMPORTED_LOCATION_RELEASE)
    if(NOT ASTREA_SETTINGS_QMAKE OR ASTREA_SETTINGS_QMAKE MATCHES "-NOTFOUND$")
        get_target_property(ASTREA_SETTINGS_QMAKE Qt6::qmake IMPORTED_LOCATION)
    endif()
endif()

if(NOT ASTREA_SETTINGS_QMAKE OR ASTREA_SETTINGS_QMAKE MATCHES "-NOTFOUND$")
    foreach(_astrea_qt_prefix IN LISTS _astrea_qt_prefix_candidates)
        foreach(_astrea_qmake_name IN ITEMS qmake6 qmake)
            if(EXISTS "${_astrea_qt_prefix}/bin/${_astrea_qmake_name}")
                set(ASTREA_SETTINGS_QMAKE "${_astrea_qt_prefix}/bin/${_astrea_qmake_name}")
                set(_astrea_selected_qt_prefix "${_astrea_qt_prefix}")
                break()
            endif()
            if(EXISTS "${_astrea_qt_prefix}/libexec/${_astrea_qmake_name}")
                set(ASTREA_SETTINGS_QMAKE "${_astrea_qt_prefix}/libexec/${_astrea_qmake_name}")
                set(_astrea_selected_qt_prefix "${_astrea_qt_prefix}")
                break()
            endif()
        endforeach()
        if(ASTREA_SETTINGS_QMAKE)
            break()
        endif()
    endforeach()
endif()

if(NOT ASTREA_SETTINGS_QMAKE OR NOT EXISTS "${ASTREA_SETTINGS_QMAKE}")
    message(FATAL_ERROR "Could not resolve qmake from the Qt6 installation selected by CMake")
endif()

execute_process(
    COMMAND "${ASTREA_SETTINGS_QMAKE}" -query QT_VERSION
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
    COMMAND "${ASTREA_SETTINGS_QMAKE}" -query QT_INSTALL_PREFIX
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

cxx_qt_import_crate(
    MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/backend/Cargo.toml"
    CXX_QT_EXPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/cxxqt"
    LOCKED
    CRATES astrea_settings_backend
    QMAKE "${ASTREA_SETTINGS_QMAKE}"
    QT_MODULES Qt6::Core Qt6::Network
)
