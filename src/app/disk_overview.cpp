#include "app/disk_overview.h"

#include "core/string_catalog.h"
#include "render/graphics_device.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <utility>

#include <d2d1_1helper.h>
#include <wrl/client.h>

namespace diskbloom::app {
namespace {

bool contains(const RectF& bounds, const float x, const float y) noexcept {
    return x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom;
}

bool same_command(
    const std::optional<OverviewCommand>& left,
    const std::optional<OverviewCommand>& right) noexcept {
    if (left.has_value() != right.has_value()) {
        return false;
    }
    return !left.has_value()
        || (left->kind == right->kind && left->volumeIndex == right->volumeIndex);
}

D2D1_RECT_F to_d2d_rect(const RectF& value) noexcept {
    return {value.left, value.top, value.right, value.bottom};
}

D2D1_COLOR_F to_d2d_color(const core::Rgba value) noexcept {
    return {value.r, value.g, value.b, value.a};
}

bool same_color(const core::Rgba left, const core::Rgba right) noexcept {
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

std::wstring format_bytes(const std::uint64_t bytes) {
    constexpr std::array units{L"B", L"KB", L"MB", L"GB", L"TB", L"PB"};
    auto value = static_cast<double>(bytes);
    std::size_t unit = 0U;
    while (value >= 1024.0 && unit + 1U < units.size()) {
        value /= 1024.0;
        ++unit;
    }

    std::array<wchar_t, 64> buffer{};
    if (unit == 0U || value >= 100.0) {
        _snwprintf_s(buffer.data(), buffer.size(), _TRUNCATE, L"%.0f %ls", value, units[unit]);
    } else {
        _snwprintf_s(buffer.data(), buffer.size(), _TRUNCATE, L"%.1f %ls", value, units[unit]);
    }
    return std::wstring(buffer.data());
}

} // namespace

OverviewLayout compute_overview_layout(
    const float widthDip,
    const float heightDip,
    const std::size_t volumeCount) {
    constexpr float title_height = 76.0F;
    constexpr float bottom_height = 72.0F;
    constexpr float row_height = 100.0F;
    constexpr float horizontal_margin = 38.0F;
    constexpr float icon_left = 58.0F;
    constexpr float icon_size = 54.0F;
    constexpr float text_left = 138.0F;
    constexpr float scan_button_width = 136.0F;
    constexpr float scan_button_height = 38.0F;
    constexpr float element_gap = 24.0F;

    const auto width = std::max(widthDip, 0.0F);
    const auto height = std::max(heightDip, title_height + bottom_height);
    const auto bottom_top = height - bottom_height;

    OverviewLayout layout{
        .titleBar = {0.0F, 0.0F, width, title_height},
        .minimizeButton = {width - 138.0F, 0.0F, width - 92.0F, title_height},
        .maximizeButton = {width - 92.0F, 0.0F, width - 46.0F, title_height},
        .closeButton = {width - 46.0F, 0.0F, width, title_height},
        .bottomBar = {0.0F, bottom_top, width, height},
        .scanFolderButton = {
            horizontal_margin,
            bottom_top + 17.0F,
            horizontal_margin + 160.0F,
            bottom_top + 55.0F,
        },
        .settingsButton = {
            width - horizontal_margin - 136.0F,
            bottom_top + 17.0F,
            width - horizontal_margin,
            bottom_top + 55.0F,
        },
        .rows = {},
    };
    layout.rows.reserve(volumeCount);

    for (std::size_t index = 0; index < volumeCount; ++index) {
        const auto top = title_height + static_cast<float>(index) * row_height;
        const auto row_bottom = std::min(top + row_height, bottom_top);
        const auto button_left = width - horizontal_margin - scan_button_width;
        const auto button_top = top + (row_height - scan_button_height) * 0.5F;
        const auto track_width = std::clamp(width * 0.23F, 160.0F, 370.0F);
        const auto track_right = button_left - element_gap;
        const auto track_left = track_right - track_width;

        layout.rows.push_back({
            .bounds = {0.0F, top, width, row_bottom},
            .iconBounds = {
                icon_left,
                top + (row_height - icon_size) * 0.5F,
                icon_left + icon_size,
                top + (row_height + icon_size) * 0.5F,
            },
            .textBounds = {
                text_left,
                top + 18.0F,
                std::max(text_left, track_left - element_gap),
                row_bottom - 18.0F,
            },
            .progressTrack = {
                track_left,
                top + 42.0F,
                track_right,
                top + 50.0F,
            },
            .scanButton = {
                button_left,
                button_top,
                button_left + scan_button_width,
                button_top + scan_button_height,
            },
        });
    }

    return layout;
}

std::optional<OverviewCommand> hit_test_overview(
    const OverviewLayout& layout,
    const float xDip,
    const float yDip) noexcept {
    for (std::size_t index = 0; index < layout.rows.size(); ++index) {
        if (contains(layout.rows[index].scanButton, xDip, yDip)) {
            return OverviewCommand{OverviewCommandKind::ScanVolume, index};
        }
    }

    if (contains(layout.scanFolderButton, xDip, yDip)) {
        return OverviewCommand{OverviewCommandKind::ScanFolder};
    }
    if (contains(layout.settingsButton, xDip, yDip)) {
        return OverviewCommand{OverviewCommandKind::OpenSettings};
    }
    if (contains(layout.minimizeButton, xDip, yDip)) {
        return OverviewCommand{OverviewCommandKind::MinimizeWindow};
    }
    if (contains(layout.maximizeButton, xDip, yDip)) {
        return OverviewCommand{OverviewCommandKind::ToggleMaximizeWindow};
    }
    if (contains(layout.closeButton, xDip, yDip)) {
        return OverviewCommand{OverviewCommandKind::CloseWindow};
    }

    return std::nullopt;
}

struct DiskOverview::Resources {
    ID2D1DeviceContext* context = nullptr;
    core::Rgba themeKey{};
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> titleBar;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> row;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> alternateRow;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> primaryText;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> secondaryText;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> track;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> button;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> buttonHover;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> border;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> accent;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> danger;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> titleFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> headingFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> detailFormat;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> buttonFormat;
};

DiskOverview::DiskOverview() = default;
DiskOverview::~DiskOverview() = default;

void DiskOverview::set_volumes(std::vector<platform::windows::VolumeSnapshot> volumes) {
    volumes_ = std::move(volumes);
    detail_text_valid_ = false;
}

bool DiskOverview::ensure_resources(
    render::GraphicsDevice& graphics,
    const core::ThemeTokens& theme,
    const core::Language language) {
    auto* context = graphics.d2d_context();
    auto* write_factory = graphics.dwrite_factory();
    if (context == nullptr || write_factory == nullptr) {
        return false;
    }

    if (resources_ != nullptr
        && resources_->context == context
        && same_color(resources_->themeKey, theme.window)) {
        return true;
    }

    auto resources = std::make_unique<Resources>();
    resources->context = context;
    resources->themeKey = theme.window;

    const auto make_brush = [&](const core::Rgba color, auto& brush) {
        return SUCCEEDED(context->CreateSolidColorBrush(to_d2d_color(color), &brush));
    };

    if (!make_brush(theme.titleBar, resources->titleBar)
        || !make_brush(theme.row, resources->row)
        || !make_brush(theme.alternateRow, resources->alternateRow)
        || !make_brush(theme.primaryText, resources->primaryText)
        || !make_brush(theme.secondaryText, resources->secondaryText)
        || !make_brush(theme.track, resources->track)
        || !make_brush(theme.button, resources->button)
        || !make_brush(theme.buttonHover, resources->buttonHover)
        || !make_brush(theme.border, resources->border)
        || !make_brush(theme.accent, resources->accent)
        || !make_brush(theme.danger, resources->danger)) {
        return false;
    }

    const auto create_format = [&](
                                   const wchar_t* family,
                                   const float size,
                                   auto& format) {
        return SUCCEEDED(write_factory->CreateTextFormat(
            family,
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size,
            language == core::Language::SimplifiedChinese ? L"zh-CN" : L"en-US",
            &format));
    };

    if (!create_format(L"Segoe UI Variable Display", 28.0F, resources->titleFormat)
        || !create_format(L"Segoe UI Variable Text", 25.0F, resources->headingFormat)
        || !create_format(L"Segoe UI Variable Text", 18.0F, resources->detailFormat)
        || !create_format(L"Segoe UI Variable Text", 19.0F, resources->buttonFormat)) {
        return false;
    }

    resources->titleFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    resources->titleFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    resources->headingFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    resources->detailFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    resources->buttonFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    resources->buttonFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    resources->buttonFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    resources_ = std::move(resources);
    return true;
}

bool DiskOverview::draw(
    render::GraphicsDevice& graphics,
    const core::ThemeTokens& theme,
    const core::Language language,
    const float widthDip,
    const float heightDip) {
    if (!ensure_resources(graphics, theme, language)) {
        return false;
    }

    if (!detail_text_valid_ || detail_language_ != language) {
        detail_text_.clear();
        detail_text_.reserve(volumes_.size());
        for (const auto& volume : volumes_) {
            auto detail = format_bytes(volume.totalBytes);
            detail.append(L"  ");
            detail.append(core::get_string(
                language,
                volume.kind == platform::windows::VolumeKind::Fixed
                    ? core::StringId::FixedDrive
                    : core::StringId::RemovableDrive));
            detail_text_.push_back(std::move(detail));
        }
        detail_language_ = language;
        detail_text_valid_ = true;
    }

    layout_ = compute_overview_layout(widthDip, heightDip, volumes_.size());
    auto* context = resources_->context;
    context->FillRectangle(to_d2d_rect(layout_.titleBar), resources_->titleBar.Get());
    context->FillRectangle(to_d2d_rect(layout_.bottomBar), resources_->titleBar.Get());

    const auto draw_text = [&](
                               const std::wstring_view text,
                               IDWriteTextFormat* format,
                               const RectF& bounds,
                               ID2D1Brush* brush) {
        context->DrawTextW(
            text.data(),
            static_cast<UINT32>(text.size()),
            format,
            to_d2d_rect(bounds),
            brush,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    };

    draw_text(
        core::get_string(language, core::StringId::AppTitle),
        resources_->titleFormat.Get(),
        {180.0F, 0.0F, std::max(180.0F, widthDip - 180.0F), 76.0F},
        resources_->primaryText.Get());

    const auto hovered = [&](const OverviewCommandKind kind, const std::size_t index) {
        return hovered_command_.has_value()
            && hovered_command_->kind == kind
            && hovered_command_->volumeIndex == index;
    };

    const auto draw_window_button = [&](
                                        const RectF& bounds,
                                        const OverviewCommandKind kind,
                                        const wchar_t symbol) {
        if (hovered(kind, std::numeric_limits<std::size_t>::max())) {
            context->FillRectangle(
                to_d2d_rect(bounds),
                kind == OverviewCommandKind::CloseWindow
                    ? resources_->danger.Get()
                    : resources_->buttonHover.Get());
        }
        const std::array text{symbol, L'\0'};
        draw_text(
            std::wstring_view(text.data(), 1U),
            resources_->buttonFormat.Get(),
            bounds,
            resources_->primaryText.Get());
    };

    draw_window_button(layout_.minimizeButton, OverviewCommandKind::MinimizeWindow, L'\u2212');
    draw_window_button(layout_.maximizeButton, OverviewCommandKind::ToggleMaximizeWindow, L'\u25a1');
    draw_window_button(layout_.closeButton, OverviewCommandKind::CloseWindow, L'\u00d7');

    for (std::size_t index = 0; index < layout_.rows.size(); ++index) {
        const auto& row_layout = layout_.rows[index];
        if (row_layout.bounds.bottom <= row_layout.bounds.top) {
            continue;
        }

        context->FillRectangle(
            to_d2d_rect(row_layout.bounds),
            index % 2U == 0U ? resources_->row.Get() : resources_->alternateRow.Get());

        const auto icon = D2D1::RoundedRect(to_d2d_rect(row_layout.iconBounds), 8.0F, 8.0F);
        context->FillRoundedRectangle(icon, resources_->button.Get());
        context->DrawRoundedRectangle(icon, resources_->border.Get(), 1.0F);
        const auto icon_center = D2D1::Point2F(
            (row_layout.iconBounds.left + row_layout.iconBounds.right) * 0.5F,
            (row_layout.iconBounds.top + row_layout.iconBounds.bottom) * 0.5F);
        context->DrawEllipse({icon_center, 10.0F, 10.0F}, resources_->accent.Get(), 3.0F);

        const RectF heading_bounds{
            row_layout.textBounds.left,
            row_layout.textBounds.top,
            row_layout.textBounds.right,
            row_layout.textBounds.top + 34.0F,
        };
        const RectF detail_bounds{
            row_layout.textBounds.left,
            row_layout.textBounds.top + 38.0F,
            row_layout.textBounds.right,
            row_layout.textBounds.bottom,
        };
        draw_text(
            volumes_[index].displayName,
            resources_->headingFormat.Get(),
            heading_bounds,
            resources_->primaryText.Get());
        draw_text(
            detail_text_[index],
            resources_->detailFormat.Get(),
            detail_bounds,
            resources_->secondaryText.Get());

        const auto track = D2D1::RoundedRect(to_d2d_rect(row_layout.progressTrack), 4.0F, 4.0F);
        context->FillRoundedRectangle(track, resources_->track.Get());
        auto used_bounds = row_layout.progressTrack;
        used_bounds.right = used_bounds.left
            + (used_bounds.right - used_bounds.left)
                * static_cast<float>(platform::windows::used_fraction(volumes_[index]));
        if (used_bounds.right > used_bounds.left) {
            context->FillRoundedRectangle(
                D2D1::RoundedRect(to_d2d_rect(used_bounds), 4.0F, 4.0F),
                resources_->accent.Get());
        }

        const auto button_brush = hovered(OverviewCommandKind::ScanVolume, index)
            ? resources_->buttonHover.Get()
            : resources_->button.Get();
        context->FillRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(row_layout.scanButton), 7.0F, 7.0F),
            button_brush);
        context->DrawRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(row_layout.scanButton), 7.0F, 7.0F),
            resources_->border.Get(),
            1.0F);
        draw_text(
            core::get_string(language, core::StringId::Scan),
            resources_->buttonFormat.Get(),
            row_layout.scanButton,
            resources_->primaryText.Get());
    }

    const auto draw_bottom_button = [&](
                                        const RectF& bounds,
                                        const OverviewCommandKind kind,
                                        const core::StringId string_id) {
        context->FillRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(bounds), 6.0F, 6.0F),
            hovered(kind, std::numeric_limits<std::size_t>::max())
                ? resources_->buttonHover.Get()
                : resources_->button.Get());
        context->DrawRoundedRectangle(
            D2D1::RoundedRect(to_d2d_rect(bounds), 6.0F, 6.0F),
            resources_->border.Get(),
            1.0F);
        draw_text(
            core::get_string(language, string_id),
            resources_->buttonFormat.Get(),
            bounds,
            resources_->primaryText.Get());
    };

    draw_bottom_button(
        layout_.scanFolderButton,
        OverviewCommandKind::ScanFolder,
        core::StringId::ScanFolder);
    draw_bottom_button(
        layout_.settingsButton,
        OverviewCommandKind::OpenSettings,
        core::StringId::Settings);
    return true;
}

bool DiskOverview::pointer_moved(const float xDip, const float yDip) {
    const auto next = hit_test_overview(layout_, xDip, yDip);
    if (same_command(next, hovered_command_)) {
        return false;
    }
    hovered_command_ = next;
    return true;
}

bool DiskOverview::pointer_left() {
    if (!hovered_command_.has_value()) {
        return false;
    }
    hovered_command_.reset();
    return true;
}

void DiskOverview::pointer_pressed(const float xDip, const float yDip) {
    pending_command_ = hit_test_overview(layout_, xDip, yDip);
}

std::optional<OverviewCommand> DiskOverview::take_command() noexcept {
    return std::exchange(pending_command_, std::nullopt);
}

} // namespace diskbloom::app
