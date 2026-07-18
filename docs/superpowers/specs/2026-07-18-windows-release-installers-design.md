# Windows Release Installers Design

## Goal

Publish native Windows x64 MSI and EXE installers as attachments whenever a
GitHub Release is published. Both installers target all users, request
elevation, and offer a desktop-shortcut option that is selected by default.

## Scope

This change adds:

- a GitHub Actions release workflow;
- deterministic Release-mode build and test steps;
- a WiX Toolset v4 MSI definition;
- an Inno Setup EXE installer definition;
- Simplified Chinese and English installer text;
- release-asset upload and manual-build artifact handling.

Code signing is intentionally excluded from the first version. The packaging
steps must remain separable so signing can be inserted before upload later.

## Release Contract

The automatic workflow is triggered only by the GitHub `release` event with
the `published` action. The release tag must match `vX.Y.Z`, where each version
component contains decimal digits. Pre-release suffixes such as `-beta.1` are
rejected.

The workflow checks out the commit referenced by the release tag and derives
`X.Y.Z` from that tag. The same version is supplied to the CMake project and
both installer definitions so executable metadata, installer metadata, and
filenames cannot drift.

The published assets are named:

- `DiskBloom-X.Y.Z-windows-x64.msi`
- `DiskBloom-X.Y.Z-windows-x64-setup.exe`

The workflow grants only `contents: write` and uploads both files to the
release that triggered the run by using the authenticated GitHub CLI. Upload
must fail if either expected asset is missing or empty. Re-running the workflow
may replace an existing asset with the same name.

## Manual Validation

The same workflow also supports `workflow_dispatch`. A manual run requires a
version input matching `X.Y.Z`, builds and verifies the packages, and stores
them as a GitHub Actions artifact. It never creates, edits, or uploads to a
GitHub Release.

This path lets packaging changes be tested before a release is published.

## Build Pipeline

The job runs on a pinned Windows x64 runner image and performs these stages in
order:

1. Validate the release tag or manual version input.
2. Install pinned packaging-tool versions.
3. Configure and build DiskBloom in Release mode with the derived version.
4. Run the complete Release CTest suite.
5. Install the application into an isolated staging directory through CMake.
6. Build the MSI from the staged files with WiX Toolset v4.
7. Build the EXE installer from the same staged files with Inno Setup.
8. Verify that both expected files exist and are non-empty.
9. Upload to the triggering Release, or upload an Actions artifact for a
   manual run.

The application is compiled only once. Both packagers consume the same staged
payload to prevent differences between the MSI and EXE contents.

## Installer Behavior

Both installers:

- install the 64-bit application for all users under
  `Program Files\DiskBloom`;
- require administrator elevation;
- register DiskBloom in Windows Apps & Features;
- provide a normal uninstall path;
- add a Start menu shortcut;
- use DiskBloom's own product name, icon, and identifiers;
- expose a desktop-shortcut option selected by default.

The desktop-shortcut prompt is localized as:

- English: `Create a desktop shortcut`
- Simplified Chinese: `是否在桌面建立快捷方式`

WiX implements the shortcut as an optional, default-enabled installer feature
with localized UI. Inno Setup implements it as a default-enabled task. Removing
or deselecting the option must leave no desktop shortcut behind.

The installer UI supports English and Simplified Chinese. It follows the
native installer theme and accessibility behavior supplied by WiX and Inno
Setup; it does not reproduce the application's custom light/dark UI because it
runs before application settings exist.

## Packaging Sources

Packaging definitions live under a dedicated `packaging/windows` directory.
Shared values such as product name, publisher, executable name, staged payload
path, architecture, version, and icon path are supplied by the workflow or a
small packaging driver script rather than copied as unrelated constants.

Stable product and upgrade identifiers are committed to the repository. The
MSI product identity changes per version while the upgrade identity remains
constant, allowing Windows Installer to recognize future upgrades.

## Failure Handling

The workflow stops without uploading assets when:

- the tag or manual version is invalid;
- configuration, compilation, or tests fail;
- WiX or Inno Setup fails;
- an expected package is absent or empty;
- release upload fails.

No partial upload is attempted until both packages have passed validation. The
workflow prints resolved version, package paths, sizes, and SHA-256 hashes to
make failures and release provenance diagnosable.

## Verification

Implementation is complete when all of the following are demonstrated:

- local Release build and complete CTest suite pass;
- a packaging script rejects invalid versions and accepts `X.Y.Z`;
- both package definitions compile on the selected GitHub runner image;
- both packages contain the staged `DiskBloom.exe` and required runtime files;
- unattended installation and uninstallation smoke checks succeed in CI;
- desktop shortcut creation is verified in the default-enabled path and its
  absence is verified when the option is disabled;
- English and Simplified Chinese prompt strings are present in their respective
  installer resources;
- manual workflow runs upload only an Actions artifact;
- published-release runs attach both packages to the triggering Release.

## Security And Future Signing

The workflow uses the repository-scoped `GITHUB_TOKEN`; it does not require a
personal access token. Tool versions are pinned, workflow permissions are
explicit, and third-party actions are minimized.

The first packages are unsigned and may trigger Windows SmartScreen warnings.
A future signing stage can consume a certificate from GitHub Secrets, sign the
application and both installers, verify signatures, and then continue to the
existing upload stage without changing the installer architecture.
