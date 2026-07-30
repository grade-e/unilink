# Changelog

All notable changes to Wirestead are documented in this file.

This project follows the Keep a Changelog section names where practical. The
core C++ API is still pre-1.0; see `docs/api_stability.md` for compatibility
and ABI policy.

## Unreleased

### Added

- Documented the Unilink to Wirestead migration path and compatibility policy.

### Changed

- Clarified that the official vcpkg port is the canonical package-consumer
  install path.
- Aligned the CMake project description with the GitHub repository
  description. This string is user-visible: it is carried into the CPack
  package metadata and the generated pkg-config description.
- Pinned the `microsoft/vcpkg` checkout used by CI, the CMake matrix, the
  release workflow, the vcpkg package test, and the `setup-vcpkg` composite
  action to a reviewed commit recorded in `VCPKG_BASELINE`, instead of cloning
  whatever is on its default branch at build time.

### Deprecated

- Unilink compatibility names remain deprecated compatibility surfaces for the
  v0.9.x line.

### Removed

- Nothing.

### Fixed

- Fixed the Windows consumer sample linking against a different MSVC runtime
  than the static vcpkg triplet its dependencies were built with, which
  surfaced as LNK2038 and LNK2005 errors after upstream vcpkg drift.

### Security

- Nothing.

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
