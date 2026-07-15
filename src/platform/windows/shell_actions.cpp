#include "platform/windows/shell_actions.h"

#include <Windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <string>

namespace diskbloom::platform::windows {
namespace {

class ItemIdList final {
public:
    explicit ItemIdList(PIDLIST_ABSOLUTE value = nullptr) noexcept : value_(value) {}
    ~ItemIdList() { ILFree(value_); }

    ItemIdList(const ItemIdList&) = delete;
    ItemIdList& operator=(const ItemIdList&) = delete;

    [[nodiscard]] PIDLIST_ABSOLUTE get() const noexcept { return value_; }
    [[nodiscard]] PIDLIST_ABSOLUTE* put() noexcept { return &value_; }

private:
    PIDLIST_ABSOLUTE value_;
};

ShellActionResult validate_path(
    const std::wstring_view path,
    DWORD& attributes,
    std::wstring& ownedPath) noexcept {
    if (path.empty()) {
        return ShellActionResult::InvalidPath;
    }
    try {
        ownedPath.assign(path);
    } catch (...) {
        return ShellActionResult::Failed;
    }
    attributes = GetFileAttributesW(ownedPath.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return ShellActionResult::Succeeded;
    }
    const auto error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND
            || error == ERROR_PATH_NOT_FOUND
            || error == ERROR_INVALID_NAME
        ? ShellActionResult::NotFound
        : ShellActionResult::Failed;
}

ShellActionResult execute_shell(
    const wchar_t* operation,
    const std::wstring& path) noexcept {
    SHELLEXECUTEINFOW execute{
        .cbSize = sizeof(SHELLEXECUTEINFOW),
        .fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC,
        .hwnd = nullptr,
        .lpVerb = operation,
        .lpFile = path.c_str(),
        .nShow = SW_SHOWNORMAL,
    };
    return ShellExecuteExW(&execute)
        ? ShellActionResult::Succeeded
        : ShellActionResult::Failed;
}

} // namespace

ShellActionResult open_with_shell(const std::wstring_view path) noexcept {
    DWORD attributes = INVALID_FILE_ATTRIBUTES;
    std::wstring ownedPath;
    const auto validation = validate_path(path, attributes, ownedPath);
    if (validation != ShellActionResult::Succeeded) {
        return validation;
    }
    return execute_shell(L"open", ownedPath);
}

ShellActionResult reveal_in_explorer(const std::wstring_view path) noexcept {
    DWORD attributes = INVALID_FILE_ATTRIBUTES;
    std::wstring ownedPath;
    const auto validation = validate_path(path, attributes, ownedPath);
    if (validation != ShellActionResult::Succeeded) {
        return validation;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
        return execute_shell(L"explore", ownedPath);
    }

    ItemIdList item;
    if (FAILED(SHParseDisplayName(ownedPath.c_str(), nullptr, item.put(), 0U, nullptr))
        || item.get() == nullptr) {
        return ShellActionResult::Failed;
    }
    ItemIdList parent(ILCloneFull(item.get()));
    if (parent.get() == nullptr || !ILRemoveLastID(parent.get())) {
        return ShellActionResult::Failed;
    }
    PCUITEMID_CHILD child = ILFindLastID(item.get());
    return SUCCEEDED(SHOpenFolderAndSelectItems(parent.get(), 1U, &child, 0U))
        ? ShellActionResult::Succeeded
        : ShellActionResult::Failed;
}

} // namespace diskbloom::platform::windows
