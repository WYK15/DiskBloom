#include "test_support.h"

#include "platform/windows/volume_service.h"

using diskbloom::platform::windows::VolumeKind;
using diskbloom::platform::windows::VolumeSnapshot;
using diskbloom::platform::windows::used_fraction;

TEST_CASE(used_fraction_reports_consumed_capacity) {
    const VolumeSnapshot volume{L"C:\\", L"System", 100U, 25U, VolumeKind::Fixed};
    CHECK(used_fraction(volume) == 0.75);
}

TEST_CASE(used_fraction_handles_empty_or_inconsistent_capacity) {
    const VolumeSnapshot empty{L"X:\\", L"Empty", 0U, 0U, VolumeKind::Removable};
    const VolumeSnapshot inconsistent{L"Y:\\", L"Odd", 100U, 120U, VolumeKind::Removable};
    CHECK(used_fraction(empty) == 0.0);
    CHECK(used_fraction(inconsistent) == 0.0);
}

TEST_CASE(used_fraction_is_one_for_full_volume) {
    const VolumeSnapshot full{L"D:\\", L"Full", 4096U, 0U, VolumeKind::Fixed};
    CHECK(used_fraction(full) == 1.0);
}
