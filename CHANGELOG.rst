^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package wirestead
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Wirestead is not a ROS package by origin; ``package.xml`` was added in v0.9.4
so the library can be released into a ROS distribution. This file exists for
the ROS release tooling, which reads reStructuredText. ``CHANGELOG.md`` remains
the full changelog and covers every release, including the ones before this
file existed.

0.9.5 (2026-08-16)
------------------
* Lower the minimum Boost version to 1.74, which is what Ubuntu 22.04 supplies,
  so the library builds on ROS 2 Humble and on RHEL 9.
* Build tests only when asked. ``WIRESTEAD_BUILD_TESTS`` now defaults to OFF, so
  a package build no longer downloads GoogleTest at configure time.
* First release a ROS build farm can build; v0.9.4 predates both changes.
* Contributors: Jinwoo Sung

0.9.4 (2026-08-16)
------------------
* Add optional TLS for the TCP client and server, off by default.
* Add UDP multicast group membership.
* Add a process-wide io thread init hook for scheduling policy and affinity.
* Add serial RS-485 mode, modem control lines, low-latency mode and a receive
  watchdog.
* Add a length-prefixed framer, which unlike the pattern framer can carry an
  arbitrary binary payload.
* Add arrival timestamps and a silence signal to the message and stats API.
* Add per-client stats on the servers.
* Fix server counters being destroyed with the session that produced them, and
  a peak queue depth reported as a sum.
* First release carrying ``package.xml``; the ROS build type is plain CMake.
* Contributors: Jinwoo Sung
