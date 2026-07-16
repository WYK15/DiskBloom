#include "app/analyzer_history.h"

namespace diskbloom::app {

void AnalyzerHistory::reset(const NavigationEntry entry) {
    if (entries_.capacity() < capacity) {
        entries_.reserve(capacity);
    }
    entries_.clear();
    entries_.push_back(entry);
    cursor_ = 0U;
}

bool AnalyzerHistory::record(const NavigationEntry entry) {
    if (entries_.empty()) {
        reset(entry);
        return true;
    }
    if (entries_[cursor_] == entry) {
        return false;
    }
    if (cursor_ + 1U < entries_.size()) {
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(cursor_ + 1U), entries_.end());
    }
    if (entries_.size() == capacity) {
        entries_.erase(entries_.begin());
        if (cursor_ > 0U) {
            --cursor_;
        }
    }
    entries_.push_back(entry);
    cursor_ = entries_.size() - 1U;
    return true;
}

std::optional<NavigationEntry> AnalyzerHistory::back() noexcept {
    if (!can_back()) {
        return std::nullopt;
    }
    return entries_[--cursor_];
}

std::optional<NavigationEntry> AnalyzerHistory::forward() noexcept {
    if (!can_forward()) {
        return std::nullopt;
    }
    return entries_[++cursor_];
}

std::optional<NavigationEntry> AnalyzerHistory::current() const noexcept {
    if (entries_.empty()) {
        return std::nullopt;
    }
    return entries_[cursor_];
}

bool AnalyzerHistory::can_back() const noexcept {
    return !entries_.empty() && cursor_ > 0U;
}

bool AnalyzerHistory::can_forward() const noexcept {
    return !entries_.empty() && cursor_ + 1U < entries_.size();
}

} // namespace diskbloom::app
