# Copies the DLLs an executable needs next to it after every build, so
# build/bin runs without Qt, MinGW or Rust folders on PATH.
#
#   qrs_copy_runtime_dlls(<target>)   Rust encoder + Qt libraries + MinGW runtime
#   qrs_deploy_qt_plugins(<target>)   Qt plugins (platforms/, styles/, imageformats/, ...)
#
# Call them after the targets they name are defined. Nothing here scans disks:
# every location comes from CMake variables and the compiler path.

# --- MinGW runtime (libgcc, libstdc++, winpthread) ---------------------------
set(QRS_MINGW_RUNTIME_DLLS "")
if(MINGW)
    get_filename_component(_qrs_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        if(EXISTS "${_qrs_compiler_dir}/${_dll}")
            list(APPEND QRS_MINGW_RUNTIME_DLLS "${_qrs_compiler_dir}/${_dll}")
        else()
            message(WARNING "MinGW runtime ${_dll} not found beside ${CMAKE_CXX_COMPILER}")
        endif()
    endforeach()
endif()

# --- windeployqt -------------------------------------------------------------
find_program(QRS_WINDEPLOYQT NAMES windeployqt6 windeployqt
    HINTS "${Qt6_DIR}/../../../bin" "${QT6_INSTALL_PREFIX}/bin"
    NO_DEFAULT_PATH)
if(NOT QRS_WINDEPLOYQT)
    find_program(QRS_WINDEPLOYQT NAMES windeployqt6 windeployqt)
endif()

function(qrs_copy_runtime_dlls target)
    # Qt libraries and the imported Rust DLL: everything the target links at run time.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs for ${target}"
        VERBATIM)

    if(QRS_MINGW_RUNTIME_DLLS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${QRS_MINGW_RUNTIME_DLLS} $<TARGET_FILE_DIR:${target}>
            VERBATIM)
    endif()
endfunction()

function(qrs_deploy_qt_plugins target)
    if(NOT QRS_WINDEPLOYQT)
        message(WARNING "windeployqt not found: Qt plugins will not be copied for ${target}")
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${QRS_WINDEPLOYQT}" --release --no-translations --no-system-d3d-compiler
                --no-opengl-sw --no-compiler-runtime --dir "$<TARGET_FILE_DIR:${target}>"
                "$<TARGET_FILE:${target}>"
        COMMENT "Deploying Qt plugins for ${target}"
        VERBATIM)
endfunction()
