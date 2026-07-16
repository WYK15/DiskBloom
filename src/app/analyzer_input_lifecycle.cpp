#include "app/analyzer_input_lifecycle.h"

namespace diskbloom::app {

AnalyzerInputActions compute_analyzer_input_actions(
    const AnalyzerInputTransition transition,
    const bool dragPending,
    const bool captureOwned) noexcept {
    AnalyzerInputActions actions;
    switch (transition) {
    case AnalyzerInputTransition::PointerDown:
        actions.beginCapture = dragPending && !captureOwned;
        break;
    case AnalyzerInputTransition::PointerReleased:
        actions.releaseCapture = captureOwned;
        actions.dispatchCommand = true;
        break;
    case AnalyzerInputTransition::PointerLeft:
        actions.releaseCapture = captureOwned;
        actions.cancelDrag = true;
        actions.clearHover = true;
        break;
    case AnalyzerInputTransition::CaptureLost:
        actions.cancelDrag = true;
        actions.cancelTransition = true;
        break;
    case AnalyzerInputTransition::ContentChanging:
        actions.releaseCapture = captureOwned;
        actions.cancelDrag = true;
        actions.cancelTransition = true;
        break;
    }
    return actions;
}

} // namespace diskbloom::app
