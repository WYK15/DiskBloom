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
    L"Open",
    L"Show in Explorer",
    L"Add to Review",
    L"Delete...",
    L"Move the reviewed items to the Recycle Bin?",
    L"Recycling...",
    L"Unable to complete the action.",
    L"Theme",
    L"Use system setting",
    L"Light",
    L"Dark",
    L"Language",
    L"English",
    L"Simplified Chinese",
    L"collected",
    L"Disks and Folders",
    L"Directory transitions",
    L"Always on",
    L"Follow Windows",
    L"Off",
    L"Drag files here to collect them for deletion",
    L"Restore item",
    L"Text size",
    L"Font",
    L"Chart size",
    L"60%",
    L"70%",
    L"80%",
    L"90%",
    L"100%",
    L"110%",
    L"120%",
    L"Segoe UI Variable",
    L"Microsoft YaHei UI",
    L"Arial",
    L"Consolas",
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
    L"\u6253\u5f00",
    L"\u5728\u8d44\u6e90\u7ba1\u7406\u5668\u4e2d\u663e\u793a",
    L"\u52a0\u5165\u5f85\u5220\u9664\u5217\u8868",
    L"\u5220\u9664...",
    L"\u5c06\u5df2\u9009\u9879\u76ee\u79fb\u5165\u56de\u6536\u7ad9\u5417\uff1f",
    L"\u6b63\u5728\u79fb\u5165\u56de\u6536\u7ad9...",
    L"\u65e0\u6cd5\u5b8c\u6210\u64cd\u4f5c\u3002",
    L"\u4e3b\u9898",
    L"\u8ddf\u968f\u7cfb\u7edf",
    L"\u4eae\u8272",
    L"\u6df1\u8272",
    L"\u8bed\u8a00",
    L"\u82f1\u6587",
    L"\u7b80\u4f53\u4e2d\u6587",
    L"\u5df2\u6536\u96c6",
    L"\u78c1\u76d8\u548c\u6587\u4ef6\u5939",
    L"\u76ee\u5f55\u5207\u6362\u52a8\u753b",
    L"\u59cb\u7ec8\u5f00\u542f",
    L"\u8ddf\u968f Windows",
    L"\u5173\u95ed",
    L"\u5c06\u6587\u4ef6\u62d6\u653e\u81f3\u6b64\uff0c\u4ee5\u6536\u96c6\u9700\u8981\u5220\u9664\u7684\u6587\u4ef6",
    L"\u6062\u590d\u9879\u76ee",
    L"\u6587\u5b57\u5927\u5c0f",
    L"\u5b57\u4f53",
    L"\u5706\u56fe\u5927\u5c0f",
    L"60%",
    L"70%",
    L"80%",
    L"90%",
    L"100%",
    L"110%",
    L"120%",
    L"Segoe UI Variable",
    L"Microsoft YaHei UI",
    L"Arial",
    L"Consolas",
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
