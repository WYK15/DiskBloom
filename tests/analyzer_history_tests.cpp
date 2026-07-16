#include "test_support.h"

#include "app/analyzer_history.h"

using diskbloom::app::AnalyzerHistory;
using diskbloom::app::MainContentView;
using diskbloom::app::NavigationEntry;
using diskbloom::core::invalid_node;

TEST_CASE(analyzer_history_supports_back_forward_and_clears_forward_on_branch) {
    AnalyzerHistory history;
    history.reset({MainContentView::Overview, invalid_node});
    CHECK(history.record({MainContentView::Analyzer, 0U}));
    CHECK(history.record({MainContentView::Analyzer, 1U}));
    CHECK((history.back() == NavigationEntry{MainContentView::Analyzer, 0U}));
    CHECK((history.forward() == NavigationEntry{MainContentView::Analyzer, 1U}));
    CHECK((history.back() == NavigationEntry{MainContentView::Analyzer, 0U}));
    CHECK(history.record({MainContentView::Analyzer, 2U}));
    CHECK(!history.can_forward());
}

TEST_CASE(analyzer_history_suppresses_duplicate_current_entries) {
    AnalyzerHistory history;
    history.reset({MainContentView::Analyzer, 7U});

    CHECK(!history.record({MainContentView::Analyzer, 7U}));
    CHECK(!history.can_back());
    CHECK(!history.can_forward());
}

TEST_CASE(analyzer_history_evicts_oldest_entry_at_capacity) {
    AnalyzerHistory history;
    history.reset({MainContentView::Overview, invalid_node});
    for (std::size_t index = 0U; index < AnalyzerHistory::capacity; ++index) {
        CHECK(history.record({MainContentView::Analyzer, static_cast<diskbloom::core::NodeIndex>(index)}));
    }

    std::size_t backCount = 0U;
    while (history.back().has_value()) {
        ++backCount;
    }
    CHECK(backCount == AnalyzerHistory::capacity - 1U);
    CHECK(!history.back().has_value());
}

TEST_CASE(analyzer_history_reset_replaces_navigation_timeline) {
    AnalyzerHistory history;
    history.reset({MainContentView::Overview, invalid_node});
    (void)history.record({MainContentView::Analyzer, 1U});
    history.reset({MainContentView::Analyzer, 9U});

    CHECK(!history.can_back());
    CHECK(!history.can_forward());
    CHECK((history.current() == NavigationEntry{MainContentView::Analyzer, 9U}));
}

TEST_CASE(analyzer_history_can_store_overview_entries) {
    AnalyzerHistory history;
    history.reset({MainContentView::Analyzer, 4U});
    CHECK(history.record({MainContentView::Overview, invalid_node}));
    CHECK((history.back() == NavigationEntry{MainContentView::Analyzer, 4U}));
    CHECK((history.forward() == NavigationEntry{MainContentView::Overview, invalid_node}));
}
