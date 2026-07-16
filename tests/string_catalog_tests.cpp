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
}

TEST_CASE(string_catalog_rejects_invalid_identifier) {
    const auto invalid = static_cast<StringId>(static_cast<std::size_t>(StringId::Count) + 1U);
    CHECK(get_string(Language::English, invalid).empty());
}
