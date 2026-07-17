#include "test_support.h"

#include "core/language.h"
#include "core/string_catalog.h"

#include <cstddef>

using diskbloom::core::Language;
using diskbloom::core::StringId;
using diskbloom::core::get_string;

TEST_CASE(string_catalog_has_every_translation) {
    for (const auto language : {Language::English, Language::SimplifiedChinese}) {
        for (std::size_t value = 0; value < static_cast<std::size_t>(StringId::Count); ++value) {
            CHECK(!get_string(language, static_cast<StringId>(value)).empty());
        }
    }
}

TEST_CASE(string_catalog_switches_language) {
    CHECK(get_string(Language::English, StringId::Scan) == L"Scan");
    CHECK(get_string(Language::SimplifiedChinese, StringId::Scan) == L"\u626b\u63cf");
    CHECK(get_string(Language::English, StringId::Collected) == L"collected");
    CHECK(get_string(Language::SimplifiedChinese, StringId::Collected)
        == L"\u5df2\u6536\u96c6");
    CHECK(get_string(Language::English, StringId::DisksAndFolders)
        == L"Disks and Folders");
    CHECK(get_string(Language::SimplifiedChinese, StringId::DisksAndFolders)
        == L"\u78c1\u76d8\u548c\u6587\u4ef6\u5939");
    CHECK(get_string(Language::English, StringId::DirectoryTransitions)
        == L"Directory transitions");
    CHECK(get_string(Language::SimplifiedChinese, StringId::DirectoryTransitions)
        == L"\u76ee\u5f55\u5207\u6362\u52a8\u753b");
    CHECK(get_string(Language::English, StringId::AnimationsAlwaysOn)
        == L"Always on");
    CHECK(get_string(Language::English, StringId::AnimationsFollowWindows)
        == L"Follow Windows");
    CHECK(get_string(Language::English, StringId::AnimationsOff) == L"Off");
    CHECK(get_string(Language::SimplifiedChinese, StringId::AnimationsAlwaysOn)
        == L"\u59cb\u7ec8\u5f00\u542f");
    CHECK(get_string(Language::SimplifiedChinese, StringId::AnimationsFollowWindows)
        == L"\u8ddf\u968f Windows");
    CHECK(get_string(Language::SimplifiedChinese, StringId::AnimationsOff)
        == L"\u5173\u95ed");
    CHECK(get_string(Language::English, StringId::CollectorDragHint)
        == L"Drag files here to collect them for deletion");
    CHECK(get_string(Language::SimplifiedChinese, StringId::CollectorDragHint)
        == L"\u5c06\u6587\u4ef6\u62d6\u653e\u81f3\u6b64\uff0c\u4ee5\u6536\u96c6\u9700\u8981\u5220\u9664\u7684\u6587\u4ef6");
    CHECK(get_string(Language::English, StringId::RestoreItem) == L"Restore item");
    CHECK(get_string(Language::SimplifiedChinese, StringId::RestoreItem)
        == L"\u6062\u590d\u9879\u76ee");
    CHECK(get_string(Language::English, StringId::TextSize) == L"Text size");
    CHECK(get_string(Language::SimplifiedChinese, StringId::TextSize)
        == L"\u6587\u5b57\u5927\u5c0f");
    CHECK(get_string(Language::English, StringId::Font) == L"Font");
    CHECK(get_string(Language::SimplifiedChinese, StringId::Font)
        == L"\u5b57\u4f53");
    CHECK(get_string(Language::English, StringId::ChartSize) == L"Chart size");
    CHECK(get_string(Language::SimplifiedChinese, StringId::ChartSize)
        == L"\u5706\u56fe\u5927\u5c0f");
    CHECK(get_string(Language::English, StringId::Percent80) == L"80%");
    CHECK(get_string(Language::SimplifiedChinese, StringId::Percent80) == L"80%");
    CHECK(get_string(Language::English, StringId::FontSegoeUiVariable)
        == L"Segoe UI Variable");
    CHECK(get_string(Language::SimplifiedChinese, StringId::FontConsolas)
        == L"Consolas");
}

TEST_CASE(string_catalog_rejects_invalid_identifier) {
    const auto invalid = static_cast<StringId>(static_cast<std::size_t>(StringId::Count) + 1U);
    CHECK(get_string(Language::English, invalid).empty());
}
