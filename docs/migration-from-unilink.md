# Migrating from UniLink to Wirestead

Wirestead is the canonical project, package, build, and C++ API identity
starting with v0.9.0. UniLink names are kept only as a v0.9.x source and build
compatibility layer for existing consumers.

The rename changes C++ mangled symbols, library filenames, and shared library
SONAMEs. Existing v0.8.x binaries are not ABI-compatible with v0.9.x and must
be rebuilt.

## Name Mapping

| Area | UniLink | Wirestead |
|---|---|---|
| Header | `<unilink/unilink.hpp>` and `<unilink/...>` | `<wirestead/wirestead.hpp>` and `<wirestead/...>` |
| Namespace | `unilink` | `wirestead` |
| CMake package | `find_package(unilink CONFIG REQUIRED)` | `find_package(wirestead CONFIG REQUIRED)` |
| CMake target | `unilink::unilink` | `wirestead::wirestead` |
| Shared/static targets | `unilink::unilink_shared`, `unilink::unilink_static` | `wirestead::wirestead_shared`, `wirestead::wirestead_static` |
| Library | `libunilink` in v0.8.x | `libwirestead` in v0.9.x |
| SONAME | `libunilink.so.0` in v0.8.x | `libwirestead.so.0` in v0.9.x |
| Export macro | `UNILINK_API` | `WIRESTEAD_API` |
| Build macros | `UNILINK_BUILD_*`, `UNILINK_ENABLE_*` | `WIRESTEAD_BUILD_*`, `WIRESTEAD_ENABLE_*` |
| Logging macros | `UNILINK_LOG_*` | `WIRESTEAD_LOG_*` |
| Environment variable | `UNILINK_LOG_LEVEL` | `WIRESTEAD_LOG_LEVEL` |
| pkg-config | `unilink.pc`, `pkg-config --libs unilink` | `wirestead.pc`, `pkg-config --libs wirestead` |
| vcpkg port | `jwsung91-unilink` compatibility alias | `wirestead` |
| Release assets | `unilink-<version>-*` | `wirestead-<version>-*` |
| Python distribution | `unilink` | `wirestead` |
| Python import | `unilink`, `unilink_py` | `wirestead` |

## Migration Steps

1. Change dependency declarations.

   For vcpkg consumers, use the official port:

   ```bash
   vcpkg install wirestead
   ```

   For CMake consumers, depend on the Wirestead package:

   ```cmake
   find_package(wirestead CONFIG REQUIRED)
   ```

2. Change include paths.

   ```cpp
   #include <wirestead/wirestead.hpp>
   ```

   Prefer the public facade include for application code. Deep includes under
   `transport/`, `interface/`, `memory/`, `concurrency/`, and `factory/` are
   not part of the pre-1.0 compatibility promise.

3. Change namespaces.

   ```cpp
   auto client = wirestead::tcp_client("127.0.0.1", 9000)
       .auto_start(false)
       .build();
   ```

4. Change CMake package and target usage.

   ```cmake
   find_package(wirestead CONFIG REQUIRED)
   target_link_libraries(my_app PRIVATE wirestead::wirestead)
   ```

5. Change macros and environment variables.

   Use `WIRESTEAD_*` CMake options, compile definitions, export macros, and
   logging macros. Use `WIRESTEAD_LOG_LEVEL` instead of `UNILINK_LOG_LEVEL`.

6. Do a clean rebuild.

   Delete CMake build directories, CMake package caches, generated build-system
   files, and any installed v0.8.x UniLink artifacts from the prefix you plan to
   use. Do not rely on an incremental relink across the rename.

7. Run tests.

   At minimum, run a build of a small external consumer with
   `find_package(wirestead CONFIG REQUIRED)`, include
   `<wirestead/wirestead.hpp>`, link `wirestead::wirestead`, and execute a local
   loopback smoke test for the transports your application uses.

## Compatibility Surface

Existing documented source usage continues to compile when rebuilt against
v0.9.x:

```cpp
#include <unilink/unilink.hpp>

unilink::builder::TcpClientBuilderDefault builder("127.0.0.1", 8080);
```

The compatibility layer is intentionally narrow. It does not support reopening
`namespace unilink { ... }`, direct forward declarations of internal UniLink
symbols, undocumented internal headers, checks that hard-code mangled symbol
names, or old shared library filenames.

Provided compatibility surfaces:

- `namespace unilink = wirestead`
- `<unilink/...>` forwarding headers
- `find_package(unilink CONFIG REQUIRED)`
- `unilink::unilink`, `unilink::unilink_shared`, and
  `unilink::unilink_static`
- `UNILINK_*` CMake option inputs for matching `WIRESTEAD_*` options
- `UNILINK_API`, `UNILINK_EXPORT`, `UNILINK_NO_EXPORT`, and `UNILINK_LOG_*`
  macro aliases
- `UNILINK_LOG_LEVEL` fallback when `WIRESTEAD_LOG_LEVEL` is unset or empty
- `unilink.pc` forwarding pkg-config metadata

`WIRESTEAD_LOG_LEVEL` takes precedence when both environment variables are set.
If both old and new CMake options are explicitly set to different values,
configure fails with `FATAL_ERROR`.

The UniLink compatibility layer is guaranteed for the v0.9.x line. Its removal
version is not fixed; removal will be decided later from real usage data and
will not make UniLink the canonical identity again.

## ABI and Install Prefixes

v0.9.0 is an ABI break from v0.8.x. Consumers must rebuild all binaries and
libraries that link to Wirestead. A `libunilink` symlink is intentionally not
provided because the C++ symbols now live under `wirestead::`; a filename-only
shim would hide the ABI break without making old binaries work.

Do not install UniLink v0.8.x and Wirestead v0.9.x into the same prefix. The
v0.9.x install includes legacy `<unilink/...>` forwarding headers and
`unilinkConfig.cmake` for source compatibility, so an old UniLink install in
the same prefix can create ambiguous headers, package configs, or stale binary
artifacts. Use separate prefixes while migrating, or remove the old install
before validating Wirestead.

## Python Users

New code should install and import Wirestead:

```bash
python -m pip uninstall -y unilink unilink_py
python -m pip install wirestead
```

```python
import wirestead
```

The `wirestead` Python package does not require the C++ source tree when a
prebuilt wheel exists for your Python version and platform. Python package and
core versions track the same minor line: `wirestead` Python 0.9.x targets
Wirestead C++ core 0.9.x.

Avoid mechanical `unilink` to `wirestead` replacement in generated files,
vendored code, historical changelog entries, issue URLs, or code that still
intentionally exercises the v0.9.x compatibility layer.

## External Repositories

External documentation, Python binding, example, benchmark, and container
repositories moved from the `unilink-lab` organization to `wirestead`, and have
since been renamed to `wirestead-docs`, `wirestead-python`,
`wirestead-examples`, `wirestead-benchmarks`, and `wirestead-container`.
