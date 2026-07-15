#include "platform/windows/recycle_bin.h"

#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wrl/client.h>

namespace diskbloom::platform::windows {
namespace {

class ComApartment final {
public:
    ComApartment() noexcept : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComApartment() {
        if (SUCCEEDED(result_)) {
            CoUninitialize();
        }
    }

    [[nodiscard]] bool available() const noexcept {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_;
};

RecycleResult validate_paths(const std::span<const std::wstring> paths) noexcept {
    if (paths.empty()) {
        return RecycleResult::InvalidPath;
    }
    for (const auto& path : paths) {
        if (path.empty() || PathIsRootW(path.c_str())) {
            return RecycleResult::InvalidPath;
        }
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            const auto error = GetLastError();
            return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
                ? RecycleResult::NotFound
                : RecycleResult::Failed;
        }
    }
    return RecycleResult::Succeeded;
}

} // namespace

RecycleResult recycle_paths(
    const std::span<const std::wstring> paths,
    const HWND ownerWindow) noexcept {
    const auto validation = validate_paths(paths);
    if (validation != RecycleResult::Succeeded) {
        return validation;
    }

    ComApartment apartment;
    if (!apartment.available()) {
        return RecycleResult::Failed;
    }

    Microsoft::WRL::ComPtr<IFileOperation> operation;
    if (FAILED(CoCreateInstance(
            CLSID_FileOperation,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&operation)))
        || FAILED(operation->SetOwnerWindow(ownerWindow))) {
        return RecycleResult::Failed;
    }

    const DWORD flags = static_cast<DWORD>(
        FOF_ALLOWUNDO
        | FOF_WANTNUKEWARNING
        | FOFX_RECYCLEONDELETE
        | FOFX_EARLYFAILURE);
    if (FAILED(operation->SetOperationFlags(flags))) {
        return RecycleResult::Failed;
    }

    for (const auto& path : paths) {
        Microsoft::WRL::ComPtr<IShellItem> item;
        if (FAILED(SHCreateItemFromParsingName(
                path.c_str(),
                nullptr,
                IID_PPV_ARGS(&item)))
            || FAILED(operation->DeleteItem(item.Get(), nullptr))) {
            return RecycleResult::Failed;
        }
    }

    if (FAILED(operation->PerformOperations())) {
        return RecycleResult::Failed;
    }
    BOOL aborted = FALSE;
    if (FAILED(operation->GetAnyOperationsAborted(&aborted))) {
        return RecycleResult::Failed;
    }
    return aborted ? RecycleResult::Cancelled : RecycleResult::Succeeded;
}

} // namespace diskbloom::platform::windows
