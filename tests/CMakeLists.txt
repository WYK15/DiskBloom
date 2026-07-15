add_executable(
    diskbloom_tests
    test_main.cpp
    test_support.cpp
)
target_link_libraries(diskbloom_tests PRIVATE diskbloom_options)
target_include_directories(diskbloom_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME diskbloom_tests COMMAND diskbloom_tests)
