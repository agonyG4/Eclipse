function(astrea_configure_layer_shell)
    if(NOT ASTREA_ENABLE_LAYER_SHELL)
        message(STATUS
            "Eclipse Layer Shell: disabled explicitly; this mode is for non-production/test builds")
        return()
    endif()

    set(ASTREA_LAYER_SHELL_QT_MIN_VERSION "6.4.5")
    find_package(LayerShellQt QUIET CONFIG)
    if(NOT LayerShellQt_FOUND)
        message(FATAL_ERROR
            "LayerShellQt >= ${ASTREA_LAYER_SHELL_QT_MIN_VERSION} is required by astrea-shell "
            "for Dock, AltTab, and Spotlight Layer Shell surfaces. No compatible package was detected. "
            "Install a Qt 6 LayerShellQt package or provide its installation through "
            "CMAKE_PREFIX_PATH, LayerShellQt_DIR, or LayerShellQt_ROOT. "
            "For non-production/test builds only, configure with -DASTREA_ENABLE_LAYER_SHELL=OFF.")
    endif()

    if(LayerShellQt_VERSION VERSION_LESS ASTREA_LAYER_SHELL_QT_MIN_VERSION)
        message(FATAL_ERROR
            "LayerShellQt >= ${ASTREA_LAYER_SHELL_QT_MIN_VERSION} is required by astrea-shell; "
            "detected LayerShellQt ${LayerShellQt_VERSION}, which is too old. "
            "Select a newer package through CMAKE_PREFIX_PATH or LayerShellQt_DIR. "
            "For non-production/test builds only, configure with -DASTREA_ENABLE_LAYER_SHELL=OFF.")
    endif()

    if(NOT TARGET LayerShellQt::Interface)
        message(FATAL_ERROR
            "The discovered LayerShellQt package is required by astrea-shell but does not export "
            "LayerShellQt::Interface. Check that a Qt 6 development package is selected through "
            "CMAKE_PREFIX_PATH or LayerShellQt_DIR.")
    endif()

    message(STATUS
        "Eclipse Layer Shell: LayerShellQt ${LayerShellQt_VERSION} found; "
        "astrea-shell Layer Shell is required")
endfunction()
