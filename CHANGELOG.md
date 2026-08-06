# Changelog

All notable changes to Wirestead are documented in this file.

This project follows the Keep a Changelog section names where practical. The
core C++ API is still pre-1.0; see `docs/api_stability.md` for compatibility
and ABI policy.

## Unreleased

### Changed

- Removed the per-chunk heap allocation and copy from the receive path.
  `MessageContext` now borrows the payload for the single-shot `on_data()` and
  `on_message()` dispatch instead of copying it into an owned buffer; only the
  batch handlers, whose contexts are queued until the batch is flushed, still
  own their data. Measured on a TCP loopback echo, allocations on the receiving
  io thread drop from 3 to 2 per callback. The documented callback contract is
  unchanged - `docs/callbacks.md` already scoped the view to the callback - and
  copying a `MessageContext` still takes ownership, so keeping one past the
  callback remains safe.
- `MessageContext::safe_data()` materializes its buffer on demand when the
  context borrows its payload. It is now the only accessor that copies; prefer
  `data()`, `data_as_string()`, or `data_as_vector()`.

## v0.9.2 - 2026-08-01

### Added

- Documented the Unilink to Wirestead migration path and compatibility policy.

### Changed

- Clarified that the official vcpkg port is the canonical package-consumer
  install path.
- Aligned the CMake project description with the GitHub repository
  description. This string is user-visible: it is carried into the CPack
  package metadata and the generated pkg-config description.
- Drive link-time optimization through CMake's `INTERPROCEDURAL_OPTIMIZATION`
  property instead of hand-written `/GL`, `/LTCG`, and `-flto` flags, applied
  per library target and only to the optimized configurations.
  `WIRESTEAD_ENABLE_LTO` now means the same thing on every compiler; it
  previously gated `-flto` on GCC and Clang while MSVC received `/GL`
  unconditionally in Release, so the option had no effect there.
- Removed the MSVC workaround that forced `/GL-` and `/LTCG:OFF` onto the
  library targets to avoid `link.exe` access violations. A CI run with the
  override removed confirmed the underlying problem no longer reproduces, and
  the flags it fought with are gone.
- Documented that out-of-line functions declared in internal headers are not
  always exported from the shared library, so code calling them links against
  the static library but not the shared one. The documented public surface is
  exported from both.
- Extended the install-and-consume CI job to macOS and Windows. It previously
  ran on Linux only, so the installed-package path was never exercised on any
  other platform.
- Added a CI check that inspects the installed shared library and fails if it
  exports no Wirestead symbols or any third-party ones. The unit tests link the
  static library, so nothing else looks at what the shared library exports.
- Pinned every `microsoft/vcpkg` checkout to a reviewed commit recorded in
  `VCPKG_BASELINE`, instead of cloning whatever is on its default branch at
  build time. This covers CI, the CMake matrix, the release workflow, the
  vcpkg package test, the install-and-consume job, and the `setup-vcpkg`
  composite action. The Windows x64 legs previously used the vcpkg that ships
  in the runner image, which meant release binaries depended on whichever
  version that image happened to carry.
- Added a weekly `vcpkg baseline bump` workflow that proposes a
  `VCPKG_BASELINE` update as a reviewable pull request, so the pin can be kept
  current without relying on someone remembering to move it.

### Deprecated

- Unilink compatibility names remain deprecated compatibility surfaces for the
  v0.9.x line.

### Fixed

- Made the TCP client retry tests wait for the attempts they assert on instead
  of running for a fixed duration and hoping. They raced anything that slowed
  the machine down, and one of them failed a coverage run on a resolve that
  took longer than the window allowed.
- Stopped the shared library from re-exporting `boost::asio::detail` symbols it
  merely links against. Those are weak and unique symbols that hidden
  visibility does not remove, so a consumer loading a different Boost into the
  same process could have them merged, which is an ODR violation. No
  `wirestead` symbol changed: the export surface this project owns, including
  the typeinfo and vtables needed to catch exceptions across the library
  boundary, is unchanged.
- Fixed the Windows consumer sample linking against a different MSVC runtime
  than the static vcpkg triplet its dependencies were built with, which
  surfaced as LNK2038 and LNK2005 errors after upstream vcpkg drift.

### Compatibility

- The shared library no longer exports symbols this project does not own.
  Every `wirestead` symbol is still exported, including those from internal
  namespaces and the typeinfo and vtables that exceptions thrown across the
  library boundary need, so no Wirestead API was removed. Code that resolved a
  `boost`, `spdlog`, or `fmt` symbol out of `libwirestead` rather than linking
  it directly must now link it directly; that was never a supported use.
- No intentional public API removal was made in v0.9.2.
- Known limitations remain the pre-1.0 ABI policy and the v0.9.x Unilink
  compatibility layer described in `docs/migration-from-unilink.md`.

## v0.9.1 - 2026-07-26

### Changed

- Linked the published `wirestead-docs` site from the README and updated
  satellite repository links to the `wirestead` organization.
- Updated vcpkg references after the `wirestead` port became available from the
  vcpkg registry.
- Moved docs and coverage Pages ownership to `wirestead-docs`.
- Updated GitHub Actions dependency pins, including `actions/setup-python`.

### Fixed

- Addressed correctness and concurrency findings in builders, configuration
  management, framers, callback guards, TCP/UDP/UDS wrappers, serial wrapper,
  and related tests.
- Fixed Doxygen checkout path handling so generated API reference content is
  produced in the docs workflow.
- Fixed dependabot auto-merge strategy.

### Compatibility

- No intentional public API removal was made in v0.9.1.
- Known limitations remain the pre-1.0 ABI policy and the v0.9.x Unilink
  compatibility layer described in `docs/migration-from-unilink.md`.

## v0.9.0 - 2026-07-19

### Changed

- Renamed the canonical project identity from Unilink to Wirestead.
- Changed the canonical C++ namespace from `unilink` to `wirestead`.
- Changed canonical include paths from `<unilink/...>` to `<wirestead/...>`.
- Changed the canonical CMake package from `unilink` to `wirestead`.
- Changed the canonical CMake target from `unilink::unilink` to
  `wirestead::wirestead`.
- Changed generated library names, release archive names, CPack metadata, and
  pkg-config metadata from Unilink names to Wirestead names.
- Changed canonical CMake options, compile definitions, export macros, and
  runtime environment variables from `UNILINK_*` names to `WIRESTEAD_*` names.
- Changed package identity to the `wirestead` vcpkg port. The former
  `jwsung91-unilink` port is a deprecated compatibility alias.

### Compatibility

- v0.9.0 is an ABI break from v0.8.x. Consumers must rebuild binaries and
  libraries that link to the C++ core.
- `namespace unilink = wirestead` is provided for source compatibility.
- `<unilink/...>` forwarding headers are installed for the v0.9.x line.
- `find_package(unilink CONFIG REQUIRED)` and `unilink::unilink` remain as
  compatibility surfaces that forward to the Wirestead package and target.
- `UNILINK_*` CMake inputs are accepted as compatibility aliases for matching
  `WIRESTEAD_*` options. Conflicting old/new values fail at configure time.
- `UNILINK_API`, `UNILINK_EXPORT`, `UNILINK_NO_EXPORT`, and Unilink logging
  macros are compatibility aliases for the Wirestead macros.
- `UNILINK_LOG_LEVEL` is read only when `WIRESTEAD_LOG_LEVEL` is unset or empty.
- A `libunilink` filename or SONAME compatibility shim is not provided because
  old v0.8.x binaries are not ABI-compatible with v0.9.x symbols.

### Fixed

- Stabilized UDP client restart lifecycle coverage before the v0.9.0 final tag.
- Made pkg-config files relocatable.
- Fixed release workflow permissions and retry behavior during the v0.9.0
  release candidate cycle.
