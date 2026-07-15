#include "platform/windows/folder_picker.h"

#include <shobjidl.h>
#include <wrl/client.h>

#include <utility>

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

} // namespace

FolderPickResult pick_folder(const HWND ownerWindow) noexcept {
    ComApartment apartment;
    if (!apartment.available()) {
        return {};
    }

    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(
            CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dialog)))) {
        return {};
    }

    FILEOPENDIALOGOPTIONS options{};
    if (FAILED(dialog->GetOptions(&options))
        || FAILED(dialog->SetOptions(
            options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST))) {
        return {};
    }

    const auto showResult = dialog->Show(ownerWindow);
    if (showResult == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return {FolderPickStatus::Cancelled, {}};
    }
    if (FAILED(showResult)) {
        return {};
    }

    Microsoft::WRL::ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item))) {
        return {};
    }

    PWSTR rawPath = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath)) || rawPath == nullptr) {
        return {};
    }
    std::wstring path(rawPath);
    CoTaskMemFree(rawPath);
    if (path.empty()) {
        return {};
    }
    return {FolderPickStatus::Selected, std::move(path)};
}

} // namespace diskbloom::platform::windows
