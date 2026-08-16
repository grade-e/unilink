# ROS 2 Support Analysis

Status: implementation started
Reviewed: 2026-08-15

> **Implementation decision:** Wirestead core will be registered directly as
> the plain-CMake ROS package `wirestead`. ROS-specific integration lives in
> the Git repository `wirestead/wirestead-ros` as package `wirestead_ros`.
> This supersedes the earlier `wirestead_vendor` proposal retained below for
> design history. Current package and release instructions live in
> `wirestead-ros/README.md` and `wirestead-ros/docs/releasing.md`.

## Executive summary

Wirestead can support ROS 2 without introducing ROS dependencies into the core
library. The recommended design is a separate ROS adapter repository or package
set that consumes the installed `wirestead::wirestead` CMake target.

The implementation assessment is:

- **Prototype and client-only MVP: relatively straightforward**
- **Lifecycle-aware bridge for every transport: moderate effort**
- **Release-quality ROS distribution support: moderate to high effort**
- **ROS 2 Humble support with system dependencies: difficult with the current
  Boost requirement**

The communication engine does not need to be rewritten. Production drivers
should link Wirestead directly and publish semantic ROS messages. A generic raw
byte bridge remains useful for diagnostics, recording, proxying, and rapid
prototyping, but should not be a mandatory data path for every driver.

## Scope

This analysis covers ROS 2 integration for the public C++20 API and the Serial,
TCP, UDP, and Unix Domain Socket transports. It does not propose adding ROS
headers or build dependencies to the Wirestead core.

The initial target should be Linux on amd64 and arm64. Windows and macOS can be
considered after the Linux package and runtime contracts are stable.

## Current readiness

Wirestead already provides most of the transport-side capabilities required by
a ROS bridge:

- an installed CMake config package and `wirestead::wirestead` target;
- client and server interfaces with explicit start and stop contracts;
- dedicated or shared Boost.Asio contexts;
- callback-based receive, connection, disconnection, error, and backpressure
  events;
- raw, line-delimited, start/end-pattern and length-prefixed framing;
- non-blocking send APIs and ownership-transfer send APIs;
- reconnect configuration and runtime statistics;
- an arrival timestamp on every received payload, so a driver can stamp a
  message from when the bytes landed rather than from when it got round to
  publishing;
- a per-channel silence age, which is what tells a driver its device stopped
  streaming while the link still reports Connected.

Delivered since this analysis was first written (2026-08-13..15):

- `wirestead_ros` diagnostics: `report_channel_stats()` maps `RuntimeStats` onto
  `diagnostic_updater`, including a STALE level for a silent link;
- a reference lifecycle driver, `serial_line_driver`, with parameters, ordered
  shutdown through `CallbackGate`, and semantic `sensor_msgs` output;
- ROS-level integration tests: the driver's lifecycle and I/O are exercised over
  a pseudo-terminal.

The following ROS-specific capabilities are not yet complete:

- a released Wirestead tag containing its new plain-CMake `package.xml`;
- ROS messages defining binary frames and server connection identity;
- composable nodes, launch files, and a documented QoS policy (the reference
  driver is a lifecycle node with parameters, but the bridge nodes are not
  built);
- bloom and rosdistro release metadata.

## Verified build integration

A local smoke test was performed on Ubuntu 24.04 with ROS 2 Jazzy, GCC 13,
CMake 3.28, and Boost 1.83. The Boost minimum has since dropped to 1.74, and
`.github/workflows/boost-floor.yml` builds and tests the floor on Ubuntu 22.04
(Boost 1.74) and CentOS Stream 9 (Boost 1.75) - the latter being what the ROS
build farm compiles against for RHEL 9.

The test performed the following operations:

1. Installed the existing Wirestead build into a temporary prefix.
2. Created a minimal `ament_cmake` package.
3. Called `find_package(wirestead CONFIG REQUIRED)`.
4. Linked an executable to `wirestead::wirestead`.
5. Included `wirestead/wirestead.hpp` and instantiated a TCP client builder.
6. Built the package with `colcon build`.

The build completed successfully. This confirms that an installed Wirestead
package can already be consumed by an ament package without changing the core
CMake project.

This test does not validate ROS runtime behavior, lifecycle transitions, or
transport I/O.

## Superseded vendor-package alternative

The initial analysis considered a `wirestead_vendor` package in a separate
`wirestead_ros` repository with the following packages. This layout is not the
selected implementation:

```text
wirestead_ros/
├── wirestead_vendor/
├── wirestead_ros/
├── wirestead_msgs/
└── wirestead_bridge/
```

These names follow REP-144 and common ROS 2 practice:

- `_vendor` identifies the package that integrates the upstream dependency;
- `<upstream>_ros` identifies the ROS integration for an upstream library;
- `_msgs` is reserved for a package containing messages, services, or actions;
- `_bridge` identifies executable adapters between Wirestead transports and
  ROS interfaces.

Avoid `wirestead_ros_driver_support`. Although syntactically valid, `support`
is not a standard ROS suffix and does not define a stable ownership boundary.
Avoid catch-all alternatives such as `utils` and `common` for the same reason.

### `wirestead_vendor`

Provide a reproducible Wirestead dependency to the ROS build farm and source
workspaces.

Responsibilities:

- pin a released Wirestead version;
- build it with tests disabled and installation enabled;
- export the installed `wirestead::wirestead` target;
- declare Boost, spdlog, and Threads dependencies;
- avoid adding Wirestead directly with `add_subdirectory()`, because a nested
  core build changes project-wide CMake cache variables and build options.

A vendor package is the simplest initial release path. In the longer term,
Wirestead can instead be published as a system package with a rosdep key.

The vendor package must vendor only Wirestead. It should use the target ROS
platform's system Boost and spdlog packages instead of embedding replacements.
This avoids symbol and installation conflicts with dependencies used by ROS and
other packages.

### `wirestead_ros`

Provide small, composable utilities for real protocol drivers:

- conversion from ROS parameters to Wirestead transport configs;
- framer construction and validation;
- ordered lifecycle start, callback drain, stop, and cleanup;
- active-state and in-flight callback guards;
- bounded worker queues for parsers that cannot run on the I/O thread;
- conversion from `RuntimeStats` to `diagnostic_updater` output;
- consistent error-to-log and error-to-diagnostic mapping.

This package should favor owned helper objects over a required base node class.
Device drivers can then choose `rclcpp::Node` or
`rclcpp_lifecycle::LifecycleNode` without inheriting through an additional
framework layer.

The package is not itself a device driver, so it should not be named
`wirestead_driver`. Device-specific packages should use names such as
`acme_lidar_driver` and depend on `wirestead_ros`.

### `wirestead_msgs`

Define transport-neutral messages used by the optional generic bridge. Normal
protocol drivers should publish standard semantic ROS messages directly rather
than routing bytes through these messages.

A proposed receive frame is:

```text
builtin_interfaces/Time received_at
uint64 session_id
uint64 connection_id
uint64 sequence
uint8[] data
```

A proposed transmit frame is:

```text
uint64 session_id
uint64 connection_id
bool broadcast
uint8[] data
```

`session_id` changes whenever the bridge transport is restarted. Servers must
validate both IDs so that a delayed ROS message cannot be delivered to a new
client that reused an old `connection_id`. Endpoint strings belong in a
separate connection event instead of every data frame.

`std_msgs/msg/UInt8MultiArray` is sufficient for a client-only prototype, but
it cannot represent server client identity or endpoint metadata and should not
be the stable general-purpose interface.

### `wirestead_bridge`

Provide optional transport-specific lifecycle nodes:

```text
SerialBridge
TcpClientBridge
TcpServerBridge
UdpBridge
UdsClientBridge
UdsServerBridge
```

Separate node types produce smaller parameter surfaces and prevent invalid
cross-transport parameter combinations. Standalone processes should be the
default for fault isolation. Component registration is an optional deployment
mode.

Recommended lifecycle mapping:

| ROS transition | Wirestead operation |
| --- | --- |
| `on_configure` | Validate parameters, construct adapter, register callbacks |
| `on_activate` | Activate publishers, then start transport and reconnect logic |
| `on_deactivate` | Stop accepting TX messages, call `stop()`, deactivate publishers |
| `on_cleanup` | Clear callbacks and destroy the adapter |
| `on_shutdown` | Perform the same ordered stop and cleanup path |

Endpoint and framing parameters should be immutable while active. Changing
them should require a deactivate/configure/activate cycle.

An active client may still be disconnected while its reconnect loop runs.
Lifecycle state and connection state are separate contracts. A server bind
failure can fail activation, while an initially unavailable reconnecting client
should normally remain active and report `DISCONNECTED` or `CONNECTING`.

### Examples and demos

Provide at least one Serial line protocol driver and one TCP request/response
driver. These examples must consume `wirestead_ros`, parse the
callback-scoped view directly when parsing is bounded, and publish semantic ROS
messages. They are the reference implementation for third-party driver authors.

Keep source examples under `wirestead_ros/examples/` initially. Create a
separate `wirestead_demos` package only when there are complete runnable demo
nodes and launch configurations worth installing. REP-144 reserves `_demos` for
that purpose; an examples-only binary package is not required for the MVP.

## Superseded repository model

One public `wirestead_ros` source repository is sufficient for ROS integration
development, source-workspace use, CI, and a single bloom release unit. A user
cloning this repository should not need to clone another ROS repository:

```text
wirestead_ros source repository
  ├── wirestead_vendor     -> obtains a pinned Wirestead core release
  ├── wirestead_ros        -> integration library used by real drivers
  ├── wirestead_msgs       -> optional raw bridge interfaces
  └── wirestead_bridge     -> optional bridge nodes
```

This does not make every related artifact part of the same Git repository:

- `wirestead/wirestead` remains the independent upstream C++ library;
- `wirestead_ros-release` is a generated bloom release repository and is not a
  development repository;
- rosdistro entries live in `ros/rosdistro`;
- device-specific drivers should normally live in their own repositories and
  depend on `wirestead_ros`.

Binary-package users only need the ROS apt repository. Installing a bridge or a
device driver should pull `wirestead_ros` and `wirestead_vendor` transitively.
They do not interact with the release repository.

## Threading and ownership contract

This is the most important runtime integration constraint.

Wirestead receive callbacks execute on a Wirestead I/O thread. The view returned
by `MessageContext::data()` is valid only during that callback.

A real driver should parse this view synchronously and publish only the final
semantic ROS message when parsing is bounded and non-blocking. This avoids a raw
ROS/DDS hop and an additional payload copy. A driver must copy using
`data_as_vector()` before returning only when it sends the payload to another
thread or queue.

Recommended generic bridge RX path:

1. Receive `MessageContext` on the Wirestead I/O thread.
2. Copy using `data_as_vector()` before the callback returns.
3. Move the bytes into the ROS message or a bounded bridge queue.
4. Publish directly if the selected `rclcpp` publisher contract has been
   verified, or wake the ROS executor through a guard condition.

Recommended TX path:

1. Receive the ROS message in a subscription callback.
2. Validate payload size and connection identity.
3. Use `try_send_move()`, `try_send_shared()`, `try_send_to()`, or
   `try_broadcast()`.
4. Increment a rejected/drop diagnostic when the call returns `false`.

The ROS executor must not call a potentially unbounded blocking Wirestead send.
Likewise, a Wirestead callback must not call a blocking send, because the same
I/O thread may be required to clear backpressure.

For the first implementation, Wirestead should retain its own I/O context and
thread. Integrating Boost.Asio directly into the ROS executor adds substantial
lifecycle and wake-up complexity without being required for functionality.

## Framing contract

Raw reads from TCP and Serial are stream chunks, not application messages. A
single write is not guaranteed to correspond to a single receive callback.

The generic bridge and driver-support framer factory should expose:

- `framing.mode`: `raw`, `line`, or `packet`;
- line delimiter, delimiter inclusion, and maximum length;
- packet start pattern, end pattern, and maximum length.

When framing is enabled, a generic bridge should publish `on_message()` results
and not raw `on_data()` chunks. A direct driver should parse the same framed view
and publish its protocol-specific semantic message.

## Parameters and QoS

Transport parameters should map directly to existing Wirestead configuration
objects where possible:

- endpoint: device, baud rate, host, bind address, port, and socket path;
- retry: interval, maximum retries, connection timeout, and idle timeout;
- buffers: read size, socket buffer sizes, and backpressure threshold;
- transport policy: TCP no-delay, keep-alive, broadcast, and address reuse;
- framing settings;
- ROS RX and TX QoS profiles.

ROS QoS and Wirestead backpressure are two separate queueing layers. A reliable
DDS QoS setting does not make the external TCP, UDP, or Serial link reliable,
and a Wirestead `Reliable` backpressure strategy does not guarantee DDS
delivery. Both policies and their combined memory limits must be documented.

Default QoS should be conservative and configurable:

- RX telemetry: sensor-data style best effort for high-rate disposable data,
  or reliable keep-last for protocol events that must not be lost;
- TX commands: reliable keep-last with a bounded depth;
- connection and diagnostic state: reliable, with transient-local durability
  only when late joiners require the latest state.

There is no universal correct default for raw byte bridges, so launch examples
should select QoS for each demonstrated protocol.

## Diagnostics and observability

Use `diagnostic_updater` and standard `diagnostic_msgs` instead of inventing a
parallel status system.

Report at least:

- configured, active, connected, or listening state;
- active client count for servers;
- last error code and message;
- reconnect activity;
- bytes and messages sent and received;
- failed sends, dropped bytes/messages, and backpressure events;
- current, pending, and peak queued bytes.

Wirestead's `RuntimeStats` already contains most queue and traffic counters, so
this part is low risk.

## Platform support

| ROS distribution | Initial support assessment |
| --- | --- |
| Lyrical / Ubuntu 26.04 | Recommended current LTS target after dedicated CI validation |
| Jazzy / Ubuntu 24.04 | Recommended first implementation target; local CMake/colcon smoke test passed |
| Kilted / Ubuntu 24.04 | Likely low incremental cost while the distribution remains supported |
| Humble / Ubuntu 22.04 | Buildable with default system Boost since the minimum dropped to 1.74; needs its own CI validation before it is claimed |

ROS 2 Lyrical is supported until May 2031 and targets Ubuntu 26.04. ROS 2 Jazzy
targets Ubuntu 24.04 and is supported until May 2029. See the ROS 2
[release documentation](https://docs.ros.org/en/kilted/Releases/Release-Lyrical-Luth.html)
and [REP-2000](https://www.ros.org/reps/rep-2000.html).

The Lyrical assessment remains provisional until Wirestead is built and tested
on Ubuntu 26.04. The successful local result applies only to Jazzy on Ubuntu
24.04.

## Implementation difficulty

| Work item | Difficulty | Reason |
| --- | --- | --- |
| Consume installed Wirestead from ament | Low | Verified with a local colcon smoke test |
| Serial/TCP client bridge | Low to medium | Direct mapping to `ChannelInterface` and ROS topics |
| Lifecycle integration | Medium | Requires strict asynchronous start, failure, and shutdown semantics |
| UDP support | Medium | Endpoint metadata and datagram semantics must remain explicit |
| TCP/UDS server bridge | Medium to high | Requires client identity, targeted send, broadcast, and disconnect races |
| Framing parameters | Medium | Existing framers help, but parameter validation and protocol contracts are required |
| Diagnostics | Low to medium | Runtime counters already exist; ROS mapping and update scheduling remain |
| ROS build-farm release | Medium to high | Vendor packaging, rosdep, bloom, and per-distribution CI are new |
| Humble compatibility | High | Ubuntu 22.04 system Boost is below Wirestead's current minimum |

Overall, the adapter is technically feasible and does not reveal a core API
blocker. A client-only MVP is straightforward. Production support becomes more
difficult mainly because of packaging, server connection semantics, bounded
queue behavior, and shutdown testing rather than transport implementation.

## Suggested delivery phases

### Phase 1: dependency and driver foundation

- Add `wirestead_vendor`.
- Add parameter, lifecycle, callback guard, and diagnostic helpers to
  `wirestead_ros`.
- Add a Serial line protocol example that parses directly into a semantic ROS
  message.
- Validate lifecycle and I/O with a pseudo-terminal serial test.

### Phase 2: client transports and optional bridge

- Add a TCP request/response driver example.
- Finalize bridge-only `wirestead_msgs`.
- Add `SerialBridge` and `TcpClientBridge`.
- Add UDP and UDS client helpers and bridge nodes.
- Add packet framing and parameter descriptors.
- Add YAML configurations and launch files.
- Test both single-threaded and multi-threaded ROS executors.

### Phase 3: servers and release quality

- Add TCP and UDS server adapters.
- Test targeted send, broadcast, reconnect, client churn, and stop races.
- Add `launch_testing`, TSAN, Fast DDS, and Cyclone DDS CI jobs.
- Add amd64 and arm64 CI for Jazzy and Lyrical.
- Prepare rosdep, bloom, and rosdistro metadata.

## Required test coverage

The following tests are required before claiming production support:

- lifecycle transition success and failure paths;
- repeated activate/deactivate and restart cycles;
- callback payload ownership after the callback returns;
- executor responsiveness under Wirestead backpressure;
- bounded queue behavior and drop accounting;
- transport connection, reconnect, bind failure, and disconnect behavior;
- server client churn and targeted send races;
- shutdown while callbacks and ROS publications are in flight;
- Serial PTY and TCP/UDP/UDS loopback integration;
- standalone and composable-node launch tests;
- ASAN and TSAN runs for shutdown and callback paths.

## Superseded vendor release notes

The source repository can be used directly in a colcon workspace without being
registered. Registration is required to make packages discoverable through the
ROS distribution metadata and installable as binary packages such as
`ros-jazzy-wirestead-ros`.

The process below follows the ROS 2
[first-time release guide](https://docs.ros.org/en/rolling/How-To-Guides/Releasing/First-Time-Release.html)
and the
[rosdistro contribution rules](https://github.com/ros/rosdistro/blob/master/CONTRIBUTING.md).

### 1. Prepare the public source repository

- Publish `wirestead_ros` at a stable public URL.
- Give every package a globally unique underscore-separated package name.
- Add `package.xml`, `CMakeLists.txt`, `LICENSE`, maintainer, description, and
  dependency metadata to every package.
- Add a `CHANGELOG.rst` to every released package.
- Keep package versions aligned for a repository-wide release unless there is a
  documented reason to version them independently.
- Pin the exact Wirestead upstream version and verify its license and notice
  files are preserved by `wirestead_vendor`.

The repository should be releasable without fetching an unpinned branch. Every
external source must be pinned by immutable version or commit and integrity
checked where the vendor mechanism supports it.

### 2. Resolve dependencies

Declare ROS dependencies in each `package.xml`, including `rclcpp`,
`rclcpp_lifecycle`, `diagnostic_updater`, message packages, and test tools.

`wirestead_vendor` is another ROS package in the same source repository, so
downstream ROS packages depend on the package name directly. It does not need a
separate rosdep key. Boost and spdlog should resolve to native system packages
already known to rosdep.

Only add a `wirestead` rosdep key later if Wirestead itself becomes available
as an accepted native package. Rosdistro strongly prefers native packages and
warns against overlaying incompatible versions of system libraries. Vendoring a
private Boost into Humble to obtain a public Humble release was rejected for
that reason, and it is now moot: the Boost minimum is 1.74, which Ubuntu 22.04
supplies.

Before release, run:

```bash
source /opt/ros/jazzy/setup.zsh
rosdep install --from-paths . --ignore-src --rosdistro jazzy -r -y
colcon build --event-handlers console_direct+
colcon test
colcon test-result --verbose
```

Repeat the build in a clean Jazzy container and the supported Lyrical target
environment. The Lyrical release must not rely only on the Jazzy result.

### 3. Prepare the release repository and tools

Join or create an appropriate ROS release team and create a release repository,
for example `wirestead_ros-release`. Install the release tools and initialize
rosdep:

```bash
sudo apt install python3-bloom python3-catkin-pkg
sudo rosdep init  # Skip when rosdep is already initialized.
rosdep update
```

Configure the GitHub credentials required by bloom according to the official
first-time release guide. Do not commit the personal access token to either
repository.

### 4. Create the upstream release

Generate and review changelogs, bump package versions, and create the release
tag:

```bash
catkin_generate_changelog
catkin_prepare_release
```

Review the generated changelogs and the version change before pushing. The
source tree must be clean, and each package version must match its changelog.

### 5. Run the first bloom release

Start with Jazzy because its local dependency and CMake consumption path is
already verified:

```bash
bloom-release --new-track --rosdistro jazzy --track jazzy wirestead_ros
```

Configure the track with the public `wirestead_ros` source repository, its main
development branch, and the `wirestead_ros-release` repository. Bloom generates
the release metadata and opens the rosdistro pull request. Review that PR for
the complete package list, versions, source URL, and release repository URL.

Create a separate Lyrical track only after its platform CI passes:

```bash
bloom-release --new-track --rosdistro lyrical --track lyrical wirestead_ros
```

### 6. Verify the ROS build farm

After the rosdistro PR is reviewed and merged, monitor the ROS 2 build farm and
distribution status pages. A merged metadata PR is not proof that every binary
package built successfully.

Successful packages normally appear first in `ros-testing`. The official guide
states that initial builds commonly take 24 to 48 hours, while promotion from
testing to the main ROS repository occurs during a distribution sync. After the
sync, users can install packages using names derived from ROS package names, for
example:

```bash
sudo apt install ros-jazzy-wirestead-ros
sudo apt install ros-jazzy-wirestead-bridge
```

ROS package documentation and index visibility are derived from rosdistro
metadata; a separate application-store registration is not required for a
normal binary release.

### 7. Publish subsequent releases

For every update:

1. Update and review each `CHANGELOG.rst`.
2. Bump package versions and create the upstream release tag.
3. Run source, dependency, and binary-consumer tests in clean environments.
4. Run bloom for each supported distribution track.
5. Review the generated rosdistro PR and monitor build-farm jobs after merge.
6. Test packages from `ros-testing` before the next sync.

Do not release a newer Wirestead ABI through the vendor package without testing
all packages in the repository against that exact version.

## Security boundary

ROS security protects the DDS/ROS side of the bridge. It does not secure the
external Wirestead transport, and the two are separate contracts: ROS support
must not imply that the entire end-to-end path is protected by SROS2.

TCP can be encrypted. A build configured with `-DWIRESTEAD_ENABLE_TLS=ON`
offers TLS for both the client and the server, with the client verifying the
server against the system trust store or a supplied CA. **UDP, Serial and UDS
send in plaintext and there is no DTLS**, so a bridge on those transports still
needs a trusted network, an external VPN or tunnel, or application-level
authentication and encryption wherever the external link crosses an untrusted
one.

Deployment documentation should say which of the two situations a given bridge
is in rather than leaving it to be assumed.

## Superseded recommendation

Proceed with a separate `wirestead_ros` repository rather than changing the
Wirestead core. Real device drivers should link Wirestead directly through
`wirestead_ros` and publish semantic ROS messages. Keep the raw
frame bridge optional so production drivers do not pay for an unnecessary DDS
hop and payload copy.

Start with Jazzy because the local toolchain and dependency versions are known
to work, while running Lyrical CI in parallel before declaring current-LTS
support.

The minimum stable deliverable should include:

- `wirestead_vendor` and `wirestead_ros`;
- a direct Serial line protocol driver example;
- parameter, framing, lifecycle, and diagnostics helpers;
- bounded non-blocking TX behavior;
- launch files and loopback/PTTY integration tests.

`wirestead_msgs`, generic bridge nodes, server support, and public ROS build-farm
release should follow after the direct driver lifecycle and callback contracts
have been exercised by the client MVP.
