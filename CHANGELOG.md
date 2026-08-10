# Changelog

All notable changes to Wirestead are documented in this file.

This project follows the Keep a Changelog section names where practical. The
core C++ API is still pre-1.0; see `docs/api_stability.md` for compatibility
and ABI policy.

## Unreleased

### Added

- `ServerInterface::client_stats(ClientId)` returns one connected client's
  `RuntimeStats`, or `std::nullopt` when the id has no session behind it.
  `stats()` reports the server as a whole and cannot say which client produced
  the traffic; the per-session counters existed already and were simply summed
  away at the aggregation boundary.

  Implemented for TCP and UDS. UDP returns `std::nullopt` - its virtual sessions
  are datagrams grouped by source endpoint and have no queues or counters of
  their own, so there is nothing per-client to report. A disconnected client also
  returns `std::nullopt`: its totals live on in `stats()`, but the session does
  not, so sample while it is connected if you need its numbers in isolation.

  This adds a virtual to `ServerInterface`, which changes the vtable and so
  breaks ABI. C++ ABI stability is not guaranteed before v1.0; see
  `docs/api_stability.md`. It has a default returning `std::nullopt`, so any
  out-of-tree implementer of the interface still compiles.

### Changed

- A server's cumulative `RuntimeStats` counters now survive the sessions that
  produced them. `TcpServer::stats()` and `UdsServer::stats()` aggregate live
  sessions, and a session's counters used to be destroyed along with the session
  on disconnect - so a server that had moved 795 MB reported zero the moment its
  last client went away, and anything sampling `stats()` on an interval watched
  throughput collapse on every disconnect. A closing session's totals are now
  folded into the server before it is erased.

  This applies to `bytes_*`, `messages_*`, `failed_sends`, `dropped_*` and
  `backpressure_events`; `max_queued_bytes` keeps the deepest queue any session
  reached. `queued_bytes`, `pending_bytes` and `backpressure_active` are
  unchanged - they describe live queues, and a closed session has none. The
  restart contract is unchanged too: `stop()` followed by `start()` still begins
  with zeroed counters.

### Fixed

- `TcpServer::stats()` and `UdsServer::stats()` reported `max_queued_bytes` as
  the sum of every live session's high-water mark. Peaks that occurred at
  different times were added together, so the server could report a queue depth
  that never existed: two sessions that each peaked at 200 KiB an hour apart
  were reported as 400 KiB. The server-wide value is now the largest depth any
  one session reached. Servers with a single connected client are unaffected.
  The remaining fields are genuine totals and keep summing.

## v0.9.3 - 2026-08-08

### Breaking

- Builder setters all return `Derived&` and mutate in place. `on_data`,
  `on_data_batch`, `on_message`, `on_message_batch` and `on_error` used to
  return a *new* builder of a different type and leave the original
  moved-from, while `on_connect`, `framer`, `auto_start` and the rest returned
  a reference — the same builder, two conventions. The `BuilderState` template
  parameter, the `Rebind` alias and the CRTP machinery behind them are gone.

  **This bought nothing.** Nothing ever read `BuilderState`; `ibuilder.hpp`
  said so itself, documenting it as "not for mandatory build gating". It only
  created a trap — take the result of `on_data` and keep using the original and
  you are working with a moved-from object, which `[[nodiscard]]` cannot catch
  — and forced 28 explicit template instantiations of otherwise identical code.

  Chained code is unaffected, which is how the quickstart and every example are
  written:

  ```cpp
  auto client = wirestead::tcp_client("127.0.0.1", 8080)
                    .on_data(...)
                    .on_error(...)
                    .build();          // unchanged
  ```

  Two forms need updating. Spelling the type with its state:

  ```cpp
  builder::TcpClientBuilder<> b{...};                    // before
  builder::TcpClientBuilder b{...};                      // after
  ```

  And capturing the result of a handler setter, which used to hand back a new
  object:

  ```cpp
  auto b = builder::UdpClientBuilder(0).on_data_batch(h); // before
  auto udp = std::move(b).auto_start(false).build();

  auto b = builder::UdpClientBuilder(0);                  // after
  b.on_data_batch(h);
  auto udp = b.auto_start(false).build();
  ```

  `TcpClientBuilderDefault` and the other `*Default` aliases still name the
  builder, so code using those keeps compiling.


### Added

- `read_buffer_size` on the TCP and UDS configs, wrappers, and builders. This
  is the userspace buffer each read fills, and it was previously a
  compile-time-fixed 4 KiB `std::array` per connection with no way to change
  it — `receive_buffer_size` only ever set the kernel's `SO_RCVBUF`. Raising it
  cuts read completions and callback dispatches on bulk transfers. The default
  is unchanged at 4 KiB, and values are clamped to
  `[MIN_READ_BUFFER_SIZE, MAX_READ_BUFFER_SIZE]` (512 B to 1 MiB). The ceiling
  is far below `MAX_SOCKET_BUFFER_SIZE` because this buffer is per connection
  and a server multiplies it by `max_connections`.
- `read_chunk` on the serial wrapper and builder. Serial already had the knob
  on its config and the transport already honoured it, but with no wrapper or
  builder surface it was reachable only by constructing a `SerialConfig` by
  hand — serial was the one transport whose read buffer could not be set the
  way TCP and UDS set `read_buffer_size`. It is the same setting under the
  older name, and is now clamped to the same
  `[MIN_READ_BUFFER_SIZE, MAX_READ_BUFFER_SIZE]` bounds, which also closes a
  hole where `read_chunk = 0` reached the transport as `rx_.resize(0)`.

### Changed

- `MemoryPool`'s `initial_pool_size` now prefills the buckets instead of being
  discarded, and its default moves from 400 to 0. **If you pass this argument
  explicitly, the pool now really does pre-allocate**; previously the value was
  ignored and every pool started empty. The default changes to 0 so that
  behaviour is preserved for callers who never set it: the buckets run 1 KiB to
  64 KiB, so honouring the old nominal 400 would have reserved roughly 8.3 MiB
  before the first `acquire()` — a cost the old signature implied and never
  charged, and one that matters on the embedded targets this library targets.
- Stream transports now drain several queued buffers into one scatter-gather
  write instead of one send syscall per queued message. Measured on a TCP
  loopback burst of 16386 small messages, send syscalls drop from 16386 to
  1025. The socket interfaces gained a buffer-sequence `async_write` overload
  with a correct default implementation, so existing implementations keep
  working unchanged.
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
- Transports and wrappers now snapshot their callbacks through a shared pointer
  instead of copying a `std::function` on every dispatch. A `std::function`
  copy heap-allocates whenever the target outgrows its small-object buffer, and
  the receive path took one such copy per chunk at each layer. Storage
  discipline is unchanged - the same mutex guards the member and the callback
  is still invoked outside the lock. Together with the change above, the TCP
  client receive path goes from 5.06 to 0.06 allocations per callback, measured
  on a loopback echo with a handler capturing 64 bytes; receiving is now
  allocation-free.

### Fixed

- `UdpChannel::on_bytes_from()` now takes `callback_mtx_` when installing the
  callback. It assigned without the lock while the strand-confined read site
  took it, against the member's own documented invariant, so replacing the
  callback on a running channel raced with the receive path.

### Compatibility

- The shared library exports 440 fewer symbols than v0.9.2: 1366 down to 1057.
  436 of those are builder template instantiations that no longer exist —
  collapsing `Rebind` removed four instantiations per builder across seven
  builders. The remaining four are `TcpServerSession` and `UdsServerSession`
  constructors whose signatures changed with the gather-write work. No
  Wirestead API was removed other than the builder state machinery described
  under Breaking.
- Consumers must rebuild, as for any pre-1.0 release; see
  `docs/api_stability.md`.

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
