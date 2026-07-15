#pragma once

#include "core/theme.h"

#include <Windows.h>
#include <d2d1_1.h>
#include <d3d11_1.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cstdint>

namespace diskbloom::render {

class GraphicsDevice final {
public:
    GraphicsDevice() = default;
    ~GraphicsDevice() = default;

    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    [[nodiscard]] bool initialize(HWND window) noexcept;
    [[nodiscard]] bool resize(std::uint32_t width, std::uint32_t height) noexcept;
    [[nodiscard]] bool begin_draw(core::Rgba clear_color) noexcept;
    [[nodiscard]] HRESULT end_draw() noexcept;

    [[nodiscard]] ID2D1DeviceContext* d2d_context() const noexcept;
    [[nodiscard]] IDWriteFactory* dwrite_factory() const noexcept;

    void discard_device_resources() noexcept;

private:
    [[nodiscard]] bool create_device_resources() noexcept;
    [[nodiscard]] bool create_window_size_resources() noexcept;
    [[nodiscard]] HRESULT recover_device(HRESULT reason) noexcept;

    HWND window_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2d_factory_;
    Microsoft::WRL::ComPtr<ID2D1Device> d2d_device_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2d_context_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2d_target_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory_;
};

} // namespace diskbloom::render
