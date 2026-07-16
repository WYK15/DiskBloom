#include "platform/windows/settings_store.h"

#include <Windows.h>
#include <ShlObj.h>

#include <array>
#include <cstddef>
#include <string_view>
#include <system_error>

namespace diskbloom::platform::windows {
namespace {

constexpr std::string_view always_on_settings =
    "DiskBloomSettings/1\ndirectoryTransitions=always\n";
constexpr std::string_view follow_system_settings =
    "DiskBloomSettings/1\ndirectoryTransitions=system\n";
constexpr std::string_view off_settings =
    "DiskBloomSettings/1\ndirectoryTransitions=off\n";

std::string_view serialized_settings(const app::DirectoryTransitionMode mode) noexcept {
    switch (mode) {
    case app::DirectoryTransitionMode::AlwaysOn:
        return always_on_settings;
    case app::DirectoryTransitionMode::FollowSystem:
        return follow_system_settings;
    case app::DirectoryTransitionMode::Off:
        return off_settings;
    }
    return {};
}

} // namespace

std::filesystem::path default_settings_path() {
    PWSTR localAppData = nullptr;
    if (FAILED(SHGetKnownFolderPath(
            FOLDERID_LocalAppData,
            KF_FLAG_DEFAULT,
            nullptr,
            &localAppData))) {
        return {};
    }
    std::filesystem::path path(localAppData);
    CoTaskMemFree(localAppData);
    return path / L"DiskBloom" / L"settings-v1.ini";
}

std::optional<app::DirectoryTransitionMode> load_directory_transition_mode(
    const std::filesystem::path& path) noexcept {
    try {
        const auto file = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return std::nullopt;
        }

        LARGE_INTEGER size{};
        constexpr LONGLONG maximum_settings_size = 512;
        if (GetFileSizeEx(file, &size) == FALSE
            || size.QuadPart <= 0
            || size.QuadPart > maximum_settings_size) {
            CloseHandle(file);
            return std::nullopt;
        }

        std::array<char, static_cast<std::size_t>(maximum_settings_size)> buffer{};
        DWORD bytesRead = 0U;
        const auto read = ReadFile(
            file,
            buffer.data(),
            static_cast<DWORD>(size.QuadPart),
            &bytesRead,
            nullptr);
        CloseHandle(file);
        if (read == FALSE || bytesRead != static_cast<DWORD>(size.QuadPart)) {
            return std::nullopt;
        }

        const std::string_view contents(buffer.data(), bytesRead);
        if (contents == always_on_settings) {
            return app::DirectoryTransitionMode::AlwaysOn;
        }
        if (contents == follow_system_settings) {
            return app::DirectoryTransitionMode::FollowSystem;
        }
        if (contents == off_settings) {
            return app::DirectoryTransitionMode::Off;
        }
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

bool save_directory_transition_mode_atomic(
    const std::filesystem::path& path,
    const app::DirectoryTransitionMode mode) noexcept {
    try {
        const auto contents = serialized_settings(mode);
        if (path.empty() || contents.empty()) {
            return false;
        }

        std::error_code directoryError;
        std::filesystem::create_directories(path.parent_path(), directoryError);
        if (directoryError) {
            return false;
        }

        auto temporaryPath = path;
        temporaryPath += L".tmp";
        DeleteFileW(temporaryPath.c_str());
        const auto file = CreateFileW(
            temporaryPath.c_str(),
            GENERIC_WRITE,
            0U,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
            nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return false;
        }

        DWORD bytesWritten = 0U;
        const auto wrote = WriteFile(
            file,
            contents.data(),
            static_cast<DWORD>(contents.size()),
            &bytesWritten,
            nullptr);
        const auto flushed = wrote != FALSE
            && bytesWritten == contents.size()
            && FlushFileBuffers(file) != FALSE;
        CloseHandle(file);
        if (!flushed) {
            DeleteFileW(temporaryPath.c_str());
            return false;
        }

        if (MoveFileExW(
                temporaryPath.c_str(),
                path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
            DeleteFileW(temporaryPath.c_str());
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace diskbloom::platform::windows
