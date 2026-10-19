# EdaTest.cmake
#
# One helper that registers a Qt Test based test binary with CTest.
#
# Every test runs
#
#   * with the offscreen platform plugin, so the whole suite works on a build
#     server without a display, and
#   * with the Qt plugin directory of the Qt the tests were built against in
#     QT_PLUGIN_PATH, so the platform plugin is found even though the test
#     binaries do not live next to the Qt installation.
#
# The application state itself is redirected by tests/TestEnvironment.cpp, which
# points PCB_WORKBENCH_HOME at a fresh temporary directory, so running the suite
# never touches the configuration of the user running it.

function(eda_add_test name group)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE eda_testsupport eda_ui Qt6::Test)
    target_compile_options(${name} PRIVATE -Wall -Wextra)

    set(_env "QT_QPA_PLATFORM=offscreen")
    if(QT6_RUNTIME_PLUGIN_DIR)
        list(APPEND _env "QT_PLUGIN_PATH=${QT6_RUNTIME_PLUGIN_DIR}")
    endif()
    if(QT6_RUNTIME_LIB_DIR OR QT6_STUB_LIB_DIR)
        list(APPEND _env "LD_LIBRARY_PATH=${QT6_RUNTIME_LIB_DIR}:${QT6_STUB_LIB_DIR}")
    endif()

    add_test(NAME ${group}/${name} COMMAND ${name})
    set_tests_properties(${group}/${name} PROPERTIES ENVIRONMENT "${_env}")
endfunction()
