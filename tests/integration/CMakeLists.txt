# Integration tests: how the subsystems behave together, still without a GUI.
eda_add_test(test_project integration)
eda_add_test(test_kicadimport integration)

# The board fixtures live next to the sources, not in the build directory.
target_compile_definitions(test_kicadimport PRIVATE
    EDA_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
