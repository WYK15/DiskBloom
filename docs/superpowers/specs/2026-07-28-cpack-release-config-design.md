# CPack Release Configuration Design

## Goal

Ensure the Windows release workflow packages the Release build output in the portable ZIP.

## Design

The workflow will pass `-C Release` to CPack. This selects the Release configuration for the Visual Studio multi-config build, so the CMake install script stages `Release\\DiskBloom.exe` and its runtime payload before ZIP creation.

The existing PowerShell workflow contract test will assert the workflow retains this CPack configuration argument. No product code, UI, localization, or package layout changes are required.

## Error Handling and Verification

The existing portable ZIP validation remains the runtime guard: it opens the created archive and rejects a package that lacks `DiskBloom.exe`. The workflow contract test prevents the omitted configuration argument from regressing.
