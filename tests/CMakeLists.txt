add_executable(
    diskbloom_tests
    appearance_settings_tests.cpp
    analyzer_layout_tests.cpp
    analyzer_child_list_tests.cpp
    analyzer_navigation_tests.cpp
    child_ranking_tests.cpp
    disk_overview_layout_tests.cpp
    file_scanner_tests.cpp
    node_path_tests.cpp
    scan_session_tests.cpp
    scan_tree_tests.cpp
    scan_ui_state_tests.cpp
    shell_actions_tests.cpp
    string_catalog_tests.cpp
    sunburst_layout_tests.cpp
    theme_tests.cpp
    test_main.cpp
    test_support.cpp
    volume_service_tests.cpp
)
target_link_libraries(
    diskbloom_tests
    PRIVATE
        diskbloom_app
        diskbloom_core
        diskbloom_options
        diskbloom_platform_windows
        diskbloom_scan
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

add_executable(diskbloom_analyzer_render_smoke analyzer_render_smoke_main.cpp)
target_link_libraries(
    diskbloom_analyzer_render_smoke
    PRIVATE
        diskbloom_app
        diskbloom_core
        diskbloom_options
        diskbloom_render
)
target_include_directories(
    diskbloom_analyzer_render_smoke
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

add_test(NAME diskbloom_analyzer_render_smoke COMMAND diskbloom_analyzer_render_smoke)
add_test(NAME diskbloom_app_smoke COMMAND DiskBloom --smoke-test)
