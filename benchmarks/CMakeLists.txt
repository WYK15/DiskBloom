add_executable(
    diskbloom_scan_tree_benchmark
    scan_tree_benchmark.cpp
)
target_link_libraries(
    diskbloom_scan_tree_benchmark
    PRIVATE
        diskbloom_core
        diskbloom_options
)
target_include_directories(
    diskbloom_scan_tree_benchmark
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

add_executable(
    diskbloom_sunburst_layout_benchmark
    sunburst_layout_benchmark.cpp
)

add_executable(
    diskbloom_scan_tree_exclusion_benchmark
    scan_tree_exclusion_benchmark.cpp
)
target_link_libraries(
    diskbloom_scan_tree_exclusion_benchmark
    PRIVATE
        diskbloom_core
        diskbloom_options
)
target_include_directories(
    diskbloom_scan_tree_exclusion_benchmark
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
add_test(
    NAME diskbloom_scan_tree_exclusion_benchmark_gate
    COMMAND diskbloom_scan_tree_exclusion_benchmark
)
set_tests_properties(
    diskbloom_scan_tree_exclusion_benchmark_gate
    PROPERTIES PASS_REGULAR_EXPRESSION "nodes=262145;excluded_roots=128"
)
target_link_libraries(
    diskbloom_sunburst_layout_benchmark
    PRIVATE
        diskbloom_core
        diskbloom_options
)
target_include_directories(
    diskbloom_sunburst_layout_benchmark
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

add_executable(
    diskbloom_review_collector_benchmark
    review_collector_benchmark.cpp
)
target_link_libraries(
    diskbloom_review_collector_benchmark
    PRIVATE
        diskbloom_app
        diskbloom_core
        diskbloom_options
)
target_include_directories(
    diskbloom_review_collector_benchmark
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

add_test(
    NAME diskbloom_review_collector_benchmark_checksum
    COMMAND diskbloom_review_collector_benchmark 1000000
)

add_executable(
    diskbloom_sunburst_transition_benchmark
    sunburst_transition_benchmark.cpp
)
target_link_libraries(
    diskbloom_sunburst_transition_benchmark
    PRIVATE
        diskbloom_core
        diskbloom_options
)
target_include_directories(
    diskbloom_sunburst_transition_benchmark
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
add_test(
    NAME diskbloom_sunburst_transition_benchmark_gate
    COMMAND diskbloom_sunburst_transition_benchmark
)
set_tests_properties(
    diskbloom_sunburst_transition_benchmark_gate
    PROPERTIES PASS_REGULAR_EXPRESSION "segments=2048"
)
set_tests_properties(
    diskbloom_review_collector_benchmark_checksum
    PROPERTIES PASS_REGULAR_EXPRESSION "checksum=11879453"
)

add_executable(
    diskbloom_analyzer_hover_pulse_benchmark
    analyzer_hover_pulse_benchmark.cpp
)
target_link_libraries(
    diskbloom_analyzer_hover_pulse_benchmark
    PRIVATE
        diskbloom_app
        diskbloom_core
        diskbloom_options
)
target_include_directories(
    diskbloom_analyzer_hover_pulse_benchmark
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
add_test(
    NAME diskbloom_analyzer_hover_pulse_benchmark_gate
    COMMAND diskbloom_analyzer_hover_pulse_benchmark
)
set_tests_properties(
    diskbloom_analyzer_hover_pulse_benchmark_gate
    PROPERTIES PASS_REGULAR_EXPRESSION "segments=2048;checksum=1024000"
)
