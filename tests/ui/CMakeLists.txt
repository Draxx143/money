# UI tests: the real widgets, driven with the offscreen platform plugin.
eda_add_test(test_mainwindow ui)

# The board fixtures live next to the sources, not in the build directory.
target_compile_definitions(test_mainwindow PRIVATE
    EDA_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
