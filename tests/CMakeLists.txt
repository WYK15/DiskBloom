add_executable(
    diskbloom_tests
    string_catalog_tests.cpp
    test_main.cpp
    test_support.cpp
)
target_link_libraries(diskbloom_tests PRIVATE diskbloom_core diskbloom_options)
target_include_directories(
    diskbloom_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
)

add_test(NAME diskbloom_tests COMMAND diskbloom_tests)
