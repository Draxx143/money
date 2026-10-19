# Test helper shared by every test binary.
add_library(eda_testsupport STATIC ../TestEnvironment.cpp)
target_link_libraries(eda_testsupport PUBLIC eda_ui)
target_include_directories(eda_testsupport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Unit tests: one binary per subsystem, no user interface, no network.
eda_add_test(test_logging unit)
eda_add_test(test_settings unit)
eda_add_test(test_projectmodel unit)
eda_add_test(test_sexpression unit)
eda_add_test(test_paths unit)
eda_add_test(test_appidentity unit)
