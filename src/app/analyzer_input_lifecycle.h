#pragma once

namespace diskbloom::app {

enum class AnalyzerInputTransition {
    PointerDown,
    PointerReleased,
    PointerLeft,
    CaptureLost,
    ContentChanging,
};

struct AnalyzerInputActions {
    bool beginCapture = false;
    bool releaseCapture = false;
    bool cancelDrag = false;
    bool clearHover = false;
    bool dispatchCommand = false;
};

[[nodiscard]] AnalyzerInputActions compute_analyzer_input_actions(
    AnalyzerInputTransition transition,
    bool dragPending,
    bool captureOwned) noexcept;

} // namespace diskbloom::app
