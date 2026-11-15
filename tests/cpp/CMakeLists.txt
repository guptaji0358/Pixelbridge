# C++ tests (Qt Test). Run through `build.bat test` or `ctest --test-dir build`.
#
#   tst_core  encode / decode: Rust encoder -> C++ renderer -> C decoder (no window)
#   tst_files content rules, CSV, search, guarded file operations (no window)
#   tst_gui   widget-level tests of the application pages

function(qrs_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE Qt6::Test)
    qrs_copy_runtime_dlls(${name})
    add_test(NAME ${name} COMMAND ${name})
    # Tests run from build/bin so qrs_encode.dll is found next to the executable.
    set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endfunction()

qrs_add_test(tst_core tst_core.cpp)
target_link_libraries(tst_core PRIVATE qrs_core qrs_decode)

# Content classification, CSV, search and file operations.
qrs_add_test(tst_files tst_files.cpp)
target_link_libraries(tst_files PRIVATE qrs_core)

if(TARGET qrs_app)
    qrs_add_test(tst_gui tst_gui.cpp)
    target_link_libraries(tst_gui PRIVATE qrs_app)

    # Qt finds plugins beside the DLLs it was loaded from (build/bin/platforms),
    # and windeployqt only deploys the "windows" platform there. ctest runs the
    # GUI test with QT_QPA_PLATFORM=offscreen, so that plugin must be there too.
    set(_qt_plugins "${Qt6_DIR}/../../../plugins")
    if(EXISTS "${_qt_plugins}/platforms/qoffscreen.dll")
        add_custom_command(TARGET tst_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:tst_gui>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qt_plugins}/platforms/qoffscreen.dll" "$<TARGET_FILE_DIR:tst_gui>/platforms"
            COMMENT "Copying Qt offscreen platform plugin for tst_gui"
            VERBATIM)
    else()
        message(WARNING "qoffscreen.dll not found under ${_qt_plugins}; tst_gui cannot run offscreen")
    endif()
endif()
