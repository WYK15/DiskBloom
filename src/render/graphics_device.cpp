#include "render/graphics_device.h"

#include <d2d1_1helper.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace diskbloom::render {
namespace {

D2D1_COLOR_F to_d2d_color(const core::Rgba color) noexcept {
    return {color.r, color.g, color.b, color.a};
}

bool is_device_lost(const HRESULT result) noexcept {
    return result == DXGI_ERROR_DEVICE_REMOVED
        || result == DXGI_ERROR_DEVICE_RESET
        || result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

} // namespace

bool GraphicsDevice::initialize(const HWND window) noexcept {
    if (window == nullptr) {
        return false;
    }

    discard_device_resources();
    window_ = window;
    return create_device_resources() && create_window_size_resources();
}

bool GraphicsDevice::create_device_resources() noexcept {
    constexpr std::array feature_levels{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL selected_level{};
    auto result = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        feature_levels.data(),
        static_cast<UINT>(feature_levels.size()),
        D3D11_SDK_VERSION,
        &d3d_device_,
        &selected_level,
        &d3d_context_);
    if (FAILED(result)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice1> dxgi_device;
    if (FAILED(d3d_device_.As(&dxgi_device))) {
        return false;
    }
    dxgi_device->SetMaximumFrameLatency(1U);

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi_device->GetAdapter(&adapter))) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&dxgi_factory)))) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swap_chain_description{};
    swap_chain_description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_description.SampleDesc.Count = 1U;
    swap_chain_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_description.BufferCount = 2U;
    swap_chain_description.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    result = dxgi_factory->CreateSwapChainForHwnd(
        d3d_device_.Get(),
        window_,
        &swap_chain_description,
        nullptr,
        nullptr,
        &swap_chain_);
    if (FAILED(result)) {
        return false;
    }
    dxgi_factory->MakeWindowAssociation(window_, DXGI_MWA_NO_ALT_ENTER);

    D2D1_FACTORY_OPTIONS factory_options{};
    result = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &factory_options,
        reinterpret_cast<void**>(d2d_factory_.GetAddressOf()));
    if (FAILED(result)) {
        return false;
    }

    result = d2d_factory_->CreateDevice(dxgi_device.Get(), &d2d_device_);
    if (FAILED(result)) {
        return false;
    }

    result = d2d_device_->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        &d2d_context_);
    if (FAILED(result)) {
        return false;
    }

    result = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwrite_factory_.GetAddressOf()));
    return SUCCEEDED(result);
}

bool GraphicsDevice::create_window_size_resources() noexcept {
    if (swap_chain_ == nullptr || d2d_context_ == nullptr || window_ == nullptr) {
        return false;
    }

    d2d_context_->SetTarget(nullptr);
    d2d_target_.Reset();

    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    if (FAILED(swap_chain_->GetBuffer(0U, IID_PPV_ARGS(&surface)))) {
        return false;
    }

    const auto dpi = static_cast<float>(std::max(GetDpiForWindow(window_), 96U));
    const auto properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(
            DXGI_FORMAT_B8G8R8A8_UNORM,
            D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpi,
        dpi);

    if (FAILED(d2d_context_->CreateBitmapFromDxgiSurface(
            surface.Get(),
            &properties,
            &d2d_target_))) {
        return false;
    }

    d2d_context_->SetTarget(d2d_target_.Get());
    d2d_context_->SetDpi(dpi, dpi);
    return true;
}

bool GraphicsDevice::resize(const std::uint32_t width, const std::uint32_t height) noexcept {
    if (width == 0U || height == 0U) {
        return true;
    }
    if (swap_chain_ == nullptr || d2d_context_ == nullptr) {
        return false;
    }

    d2d_context_->SetTarget(nullptr);
    d2d_target_.Reset();
    const auto result = swap_chain_->ResizeBuffers(
        0U,
        width,
        height,
        DXGI_FORMAT_UNKNOWN,
        0U);
    if (is_device_lost(result)) {
        return SUCCEEDED(recover_device(result));
    }
    return SUCCEEDED(result) && create_window_size_resources();
}

bool GraphicsDevice::begin_draw(const core::Rgba clear_color) noexcept {
    if (d2d_context_ == nullptr || d2d_target_ == nullptr) {
        return false;
    }

    d2d_context_->BeginDraw();
    d2d_context_->SetTransform(D2D1::Matrix3x2F::Identity());
    d2d_context_->Clear(to_d2d_color(clear_color));
    return true;
}

HRESULT GraphicsDevice::end_draw(CapturedFrame* const capture) noexcept {
    if (d2d_context_ == nullptr || swap_chain_ == nullptr) {
        return E_UNEXPECTED;
    }

    const auto draw_result = d2d_context_->EndDraw();
    if (draw_result == D2DERR_RECREATE_TARGET) {
        return create_window_size_resources() ? S_FALSE : draw_result;
    }
    if (FAILED(draw_result)) {
        return is_device_lost(draw_result) ? recover_device(draw_result) : draw_result;
    }

    if (capture != nullptr) {
        const auto captureResult = capture_back_buffer(*capture);
        if (FAILED(captureResult)) {
            return captureResult;
        }
    }

    const auto present_result = swap_chain_->Present(1U, 0U);
    return is_device_lost(present_result)
        ? recover_device(present_result)
        : present_result;
}

HRESULT GraphicsDevice::capture_back_buffer(CapturedFrame& capture) noexcept {
    capture = {};
    if (swap_chain_ == nullptr || d3d_device_ == nullptr || d3d_context_ == nullptr) {
        return E_UNEXPECTED;
    }
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    auto result = swap_chain_->GetBuffer(0U, IID_PPV_ARGS(&backBuffer));
    if (FAILED(result)) {
        return result;
    }
    D3D11_TEXTURE2D_DESC description{};
    backBuffer->GetDesc(&description);
    if (description.Width == 0U || description.Height == 0U
        || description.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
        return E_UNEXPECTED;
    }
    description.BindFlags = 0U;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.Usage = D3D11_USAGE_STAGING;
    description.MiscFlags = 0U;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    result = d3d_device_->CreateTexture2D(&description, nullptr, &staging);
    if (FAILED(result)) {
        return result;
    }
    d3d_context_->CopyResource(staging.Get(), backBuffer.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    result = d3d_context_->Map(staging.Get(), 0U, D3D11_MAP_READ, 0U, &mapped);
    if (FAILED(result)) {
        return result;
    }

    capture.width = description.Width;
    capture.height = description.Height;
    capture.rowPitch = description.Width * 4U;
    try {
        capture.pixels.resize(
            static_cast<std::size_t>(capture.rowPitch) * capture.height);
    } catch (...) {
        d3d_context_->Unmap(staging.Get(), 0U);
        capture = {};
        return E_OUTOFMEMORY;
    }
    const auto* source = static_cast<const std::byte*>(mapped.pData);
    for (std::uint32_t row = 0U; row < capture.height; ++row) {
        std::memcpy(
            capture.pixels.data() + static_cast<std::size_t>(row) * capture.rowPitch,
            source + static_cast<std::size_t>(row) * mapped.RowPitch,
            capture.rowPitch);
    }
    d3d_context_->Unmap(staging.Get(), 0U);
    return S_OK;
}

HRESULT GraphicsDevice::recover_device(const HRESULT reason) noexcept {
    const auto window = window_;
    discard_device_resources();
    return initialize(window) ? S_FALSE : reason;
}

ID2D1DeviceContext* GraphicsDevice::d2d_context() const noexcept {
    return d2d_context_.Get();
}

IDWriteFactory* GraphicsDevice::dwrite_factory() const noexcept {
    return dwrite_factory_.Get();
}

void GraphicsDevice::discard_device_resources() noexcept {
    if (d2d_context_ != nullptr) {
        d2d_context_->SetTarget(nullptr);
    }
    d2d_target_.Reset();
    d2d_context_.Reset();
    d2d_device_.Reset();
    d2d_factory_.Reset();
    dwrite_factory_.Reset();
    swap_chain_.Reset();
    d3d_context_.Reset();
    d3d_device_.Reset();
    window_ = nullptr;
}

} // namespace diskbloom::render
