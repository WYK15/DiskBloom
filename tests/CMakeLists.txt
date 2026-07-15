add_executable(
    diskbloom_tests
    string_catalog_tests.cpp
    theme_tests.cpp
    test_main.cpp
    test_support.cpp
    volume_service_tests.cpp
)
target_link_libraries(
    diskbloom_tests
    PRIVATE
        diskbloom_core
        diskbloom_options
        diskbloom_platform_windows
)
target_include_directories(
    diskbloom_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
)

add_test(NAME diskbloom_tests COMMAND diskbloom_tests)

add_executable(diskbloom_render_smoke render_smoke_main.cpp)
target_link_libraries(
    diskbloom_render_smoke
    PRIVATE
        diskbloom_core
        diskbloom_options
        diskbloom_render
)
target_include_directories(diskbloom_render_smoke PRIVATE ${PROJECT_SOURCE_DIR}/src)

add_test(NAME diskbloom_render_smoke COMMAND diskbloom_render_smoke)
