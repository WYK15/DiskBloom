#include "core/string_catalog.h"

#include <array>
#include <cstddef>

namespace diskbloom::core {
namespace {

constexpr auto string_count = static_cast<std::size_t>(StringId::Count);

constexpr std::array<std::wstring_view, string_count> english{
    L"DiskBloom",
    L"Scan",
    L"Cancel",
    L"Scan Folder...",
    L"Settings",
    L"{0} used of {1}",
    L"Fixed drive",
    L"Removable drive",
    L"Scanning...",
    L"Cancelling...",
    L"Scan complete",
    L"Scan cancelled",
    L"Scan failed",
    L"Back",
    L"Overview",
    L"items",
    L"Other items",
    L"Theme",
    L"Use system setting",
    L"Light",
    L"Dark",
    L"Language",
    L"English",
    L"Simplified Chinese",
};

constexpr std::array<std::wstring_view, string_count> simplified_chinese{
    L"\u0044\u0069\u0073\u006b\u0042\u006c\u006f\u006f\u006d",
    L"\u626b\u63cf",
    L"\u53d6\u6d88",
    L"\u626b\u63cf\u6587\u4ef6\u5939...",
    L"\u8bbe\u7f6e",
    L"\u5df2\u4f7f\u7528 {0}\uff0c\u5171 {1}",
    L"\u672c\u5730\u78c1\u76d8",
    L"\u53ef\u79fb\u52a8\u78c1\u76d8",
    L"\u6b63\u5728\u626b\u63cf...",
    L"\u6b63\u5728\u53d6\u6d88...",
    L"\u626b\u63cf\u5b8c\u6210",
    L"\u5df2\u53d6\u6d88\u626b\u63cf",
    L"\u626b\u63cf\u5931\u8d25",
    L"\u8fd4\u56de",
    L"\u78c1\u76d8\u6982\u89c8",
    L"\u9879",
    L"\u5176\u4ed6\u9879\u76ee",
    L"\u4e3b\u9898",
    L"\u8ddf\u968f\u7cfb\u7edf",
    L"\u4eae\u8272",
    L"\u6df1\u8272",
    L"\u8bed\u8a00",
    L"\u82f1\u6587",
    L"\u7b80\u4f53\u4e2d\u6587",
};

static_assert(english.size() == string_count);
static_assert(simplified_chinese.size() == string_count);

} // namespace

std::wstring_view get_string(const Language language, const StringId id) noexcept {
    const auto index = static_cast<std::size_t>(id);
    if (index >= string_count) {
        return {};
    }

    return language == Language::SimplifiedChinese
        ? simplified_chinese[index]
        : english[index];
}

} // namespace diskbloom::core
