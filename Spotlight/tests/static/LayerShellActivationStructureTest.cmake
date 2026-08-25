if(NOT DEFINED SPOTLIGHT_SOURCE_DIR)
    message(FATAL_ERROR "SPOTLIGHT_SOURCE_DIR is required")
endif()

get_filename_component(ECLIPSE_SOURCE_DIR "${SPOTLIGHT_SOURCE_DIR}" DIRECTORY)

function(check_controller_activation source_path controller_name)
    file(READ "${source_path}" controller_source)

    string(REGEX MATCHALL "requestActivate\\(\\)" activation_calls "${controller_source}")
    list(LENGTH activation_calls activation_call_count)
    if(NOT activation_call_count EQUAL 0)
        message(FATAL_ERROR
            "${controller_name} must not call LayerShellQt requestActivate(); found ${activation_call_count}")
    endif()

    string(REGEX MATCH "onVisibleChanged:[^}]*requestActivate\\(\\)"
        visible_activation_route "${controller_source}")
    if(visible_activation_route)
        message(FATAL_ERROR
            "${controller_name} must not activate from Window.onVisibleChanged before LayerShell configure")
    endif()

    string(FIND "${controller_source}" "function onFocusRequested()" focus_handler_position)
    if(focus_handler_position EQUAL -1)
        message(FATAL_ERROR "${controller_name} activation must be owned by onFocusRequested()")
    endif()

    string(FIND "${controller_source}" "function onFrameSwapped()" frame_handler_position)
    if(frame_handler_position EQUAL -1)
        message(FATAL_ERROR "${controller_name} activation must wait for the first rendered frame")
    endif()

    string(FIND "${controller_source}" "window.focusPending = false" focus_pending_reset_position)
    if(focus_pending_reset_position EQUAL -1)
        message(FATAL_ERROR "${controller_name} must consume the pending focus request after readiness")
    endif()
endfunction()

check_controller_activation(
    "${SPOTLIGHT_SOURCE_DIR}/qml/Main.qml"
    "Spotlight")
check_controller_activation(
    "${ECLIPSE_SOURCE_DIR}/AltTab/qml/Main.qml"
    "AltTab")

file(READ "${ECLIPSE_SOURCE_DIR}/shared/platform/wayland/LayerShellHelper.cpp"
     layer_shell_helper_source)
string(FIND "${layer_shell_helper_source}" "layerWindow->setActivateOnShow(false);"
    disabled_auto_activation_position)
if(disabled_auto_activation_position EQUAL -1)
    message(FATAL_ERROR "LayerShellQt automatic activation must be disabled for controller-owned focus")
endif()

message(STATUS "Layer Shell activation structure invariants passed")
