/*
 * Copyright 2025 Jinwoo Sung
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <string>

#include "test_utils.hpp"
#include "wirestead/wirestead.hpp"

using namespace wirestead;
using namespace wirestead::test;
using namespace std::chrono_literals;

// docs/performance_validation.md gates on "unexpected allocation-count
// increase". Unlike throughput, allocation count is deterministic: counting
// global operator new on the receiving io thread gives byte-identical numbers
// across runs, which is what makes it usable as a regression gate at all.
//
// This file must stay its own executable. The counter below replaces global
// operator new for the whole binary, and the integration CMakeLists builds one
// executable per test file, so nothing else is affected.

namespace {

// thread_local so the receiving io thread is counted in isolation - gtest and
// the main thread allocate freely and are never read.
thread_local size_t t_allocs = 0;

struct ReceiveProbe {
  size_t allocs_at_first_callback = 0;
  size_t allocs_at_last_callback = 0;
  std::atomic<size_t> callbacks{0};
  std::atomic<size_t> bytes{0};
};

ReceiveProbe g_probe;

constexpr int kMessages = 1000;
constexpr size_t kPayload = 256;

// A handler must capture more than std::function's small-buffer budget for the
// shared-pointer snapshot to be observable at all: a small lambda fits the SBO
// and its copy never allocated, so a trivial handler measures nothing and looks
// like the snapshot change did nothing.
constexpr size_t kCaptureBytes = 64;

}  // namespace

void* operator new(std::size_t n) {
  ++t_allocs;
  void* p = std::malloc(n ? n : 1);
  if (!p) throw std::bad_alloc();
  return p;
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
  ++t_allocs;
  return std::malloc(n ? n : 1);
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return ::operator new(n, std::nothrow); }

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }

// The receive path must not add an allocation per delivered message. It used to
// allocate three times per callback - the chunk was copied into the context and
// each std::function handler was copied on the way out. Both are gone; this
// pins that down, because the cost is invisible in latency and only shows up as
// allocator pressure under load.
//
// Measured on the parent commit vs here: 3.0 -> 0.0 on Linux and macOS,
// 7.0 -> 3.0 and 3.0 -> 1.0 on the two Windows job configurations. What is left
// on Windows is asio's, not ours; see the bound below.
TEST(ReceiveAllocationsTest, DoesNotAllocatePerDeliveredMessage) {
  const uint16_t port = TestUtils::getAvailableTestPort();

  auto server = wirestead::tcp_server(port).build();
  server->on_data([](const wirestead::MessageContext&) {});
  ASSERT_TRUE(server->start_sync());

  std::array<char, kCaptureBytes> padding{};
  padding.fill('x');

  auto client = wirestead::tcp_client("127.0.0.1", port).max_retries(50).build();
  client->on_data([padding](const wirestead::MessageContext& ctx) {
    // Read the capture so it cannot be optimised out of the closure.
    if (padding[0] == 'y') return;
    if (g_probe.callbacks.load(std::memory_order_relaxed) == 0) g_probe.allocs_at_first_callback = t_allocs;
    g_probe.callbacks.fetch_add(1, std::memory_order_relaxed);
    g_probe.bytes.fetch_add(ctx.data().size(), std::memory_order_relaxed);
    g_probe.allocs_at_last_callback = t_allocs;
  });
  ASSERT_TRUE(client->start_sync());

  // start_sync() only promises the client's side of the handshake. The server
  // registers the session on its own io thread, and broadcast() with no
  // sessions accepts nothing - which fails this test for a reason that has
  // nothing to do with allocations.
  ASSERT_TRUE(TestUtils::waitForCondition([&] { return server->client_count() > 0; }, 5000))
      << "server never registered the connection";

  const std::string payload(kPayload, 'a');
  // broadcast() is a non-blocking fan-out, so a tight loop legitimately drops
  // once the queue fills. Count what it accepted rather than what was offered -
  // exact byte accounting is test_gather_write's job, not this test's.
  size_t accepted_bytes = 0;
  for (int i = 0; i < kMessages; ++i) {
    if (server->broadcast(payload)) accepted_bytes += payload.size();
  }
  ASSERT_GT(accepted_bytes, 0u) << "server accepted nothing to send";

  ASSERT_TRUE(TestUtils::waitForCondition(
      [&] { return g_probe.bytes.load(std::memory_order_relaxed) >= accepted_bytes; }, 15000))
      << "only " << g_probe.bytes.load() << " of " << accepted_bytes << " accepted bytes arrived";

  const size_t callbacks = g_probe.callbacks.load(std::memory_order_relaxed);
  ASSERT_GT(callbacks, 1u) << "need at least two callbacks to measure the steady state";

  // Measured between the first and last callback so connection setup, which
  // legitimately allocates, is excluded.
  const size_t allocations = g_probe.allocs_at_last_callback - g_probe.allocs_at_first_callback;
  const double per_callback = static_cast<double>(allocations) / static_cast<double>(callbacks - 1);

  // On Linux and macOS the steady state measures 0.0 exactly, so the bound only
  // has to be loose enough that a stray allocation from an unrelated io-thread
  // wakeup does not fail the build.
  //
  // Windows has a non-zero floor that is not ours to remove. asio's backend
  // there is IOCP, a proactor that allocates per async operation, and this
  // project binds no custom handler allocator, so those allocations land in the
  // same counter. The floor also depends on how the test binary is linked -
  // measured 1.0 for the x64-windows jobs and 3.0 for x64-windows-static-md -
  // so a single tight bound would either fail on one or go blind on the other.
  //
  // 3.5 clears the higher floor and still catches the regression this exists
  // for: three allocations per message coming back reads 4.0 against the 1.0
  // floor and 7.0 against the 3.0 floor, both measured on the parent commit.
  // The margin over the 3.0 floor is thin. If this starts flaking on Windows,
  // raising the bound is the wrong fix - re-measure the floor on the parent
  // commit first, because a floor that moved on its own is the finding.
#ifdef _WIN32
  constexpr double kMaxAllocationsPerCallback = 3.5;
#else
  constexpr double kMaxAllocationsPerCallback = 0.5;
#endif

  EXPECT_LT(per_callback, kMaxAllocationsPerCallback)
      << "receive path allocated " << per_callback << " times per callback (" << allocations << " allocations across "
      << callbacks << " callbacks) - something on the receive path is allocating per message again";

  client->stop();
  server->stop();
}
