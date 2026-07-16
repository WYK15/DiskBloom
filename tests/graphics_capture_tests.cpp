#include "test_support.h"

#include "core/theme.h"
#include "render/graphics_device.h"

#include <Windows.h>

#include <algorithm>
#include <cstddef>

TEST_CASE(graphics_capture_reads_bgra_back_buffer_before_present) {
    const auto window = CreateWindowExW(
        0U,
        L"STATIC",
        L"DiskBloom graphics capture test",
        WS_OVERLAPPED,
        0,
        0,
        160,
        120,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    CHECK(window != nullptr);

    diskbloom::render::GraphicsDevice graphics;
    CHECK(graphics.initialize(window));
    constexpr diskbloom::core::Rgba clear{0.2F, 0.4F, 0.6F, 1.0F};
    CHECK(graphics.begin_draw(clear));
    diskbloom::render::CapturedFrame frame;
    CHECK(SUCCEEDED(graphics.end_draw(&frame)));
    CHECK(frame.width > 0U);
    CHECK(frame.height > 0U);
    CHECK(frame.rowPitch == frame.width * 4U);
    CHECK(frame.pixels.size()
        == static_cast<std::size_t>(frame.rowPitch) * frame.height);

    const auto offset = (static_cast<std::size_t>(frame.height / 2U) * frame.rowPitch)
        + static_cast<std::size_t>(frame.width / 2U) * 4U;
    const auto close = [&](const std::byte value, const unsigned expected) {
        const auto actual = std::to_integer<unsigned>(value);
        return actual >= expected - 2U && actual <= expected + 2U;
    };
    CHECK(close(frame.pixels[offset + 0U], 153U));
    CHECK(close(frame.pixels[offset + 1U], 102U));
    CHECK(close(frame.pixels[offset + 2U], 51U));
    CHECK(close(frame.pixels[offset + 3U], 255U));
    DestroyWindow(window);
}

