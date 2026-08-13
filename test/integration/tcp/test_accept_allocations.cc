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

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

#include "test_utils.hpp"
#include "wirestead/wirestead.hpp"

using namespace wirestead;
using namespace wirestead::test;
using namespace std::chrono_literals;

// Accepting a connection must not eagerly allocate a megabyte.
//
// TcpServerSession holds a per-session memory::MemoryPool. Its
// initial_pool_size argument was written while MemoryPool discarded that
// parameter, so the 50 it passed allocated nothing. #575 made the parameter
// real without revisiting the call sites, which turned every accept into
// ~1 MiB of prefill across 48 new[] calls - charged synchronously on the
// accept handler, before the next async_accept is issued, and paid again for
// every connected client (1 GiB at the default 1024 connection limit).
//
// Like test_receive_allocations.cc this must stay its own executable: the
// counter below replaces global operator new for the whole binary, and the
// integration CMakeLists builds one executable per test file.

namespace {

// thread_local so the server's io thread is measured in isolation. The server
// transport runs its io_context on a single jthread, so every accept lands on
// the same thread and the counter is comparable across connections. gtest, the
// main thread and each client's own io thread allocate freely and are never
// read.
thread_local size_t t_bytes = 0;

struct AcceptProbe {
  std::atomic<size_t> connects{0};
  size_t bytes_at_first_connect = 0;
  size_t bytes_at_second_connect = 0;
};

AcceptProbe g_probe;

}  // namespace

void* operator new(std::size_t n) {
  t_bytes += n;
  void* p = std::malloc(n ? n : 1);
  if (!p) throw std::bad_alloc();
  return p;
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
  t_bytes += n;
  return std::malloc(n ? n : 1);
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return ::operator new(n, std::nothrow); }

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }

TEST(AcceptAllocationsTest, DoesNotEagerlyAllocatePerAcceptedSession) {
  const uint16_t port = TestUtils::getAvailableTestPort();

  auto server = wirestead::tcp_server(port).build();
  server->on_connect([](const wirestead::ConnectionContext&) {
    // Runs on the server's io thread, from the accept handler that just
    // constructed the session - so t_bytes here already includes that
    // session's construction.
    //
    // Snapshot first, then publish the count with release against the waiter's
    // acquire below. The main thread reads these plain members as soon as it
    // observes the count, so incrementing first would let it read a slot this
    // thread has not written yet - which underflows the subtraction below into
    // a spurious multi-megabyte failure. The relaxed load is safe because the
    // server runs its io_context on one thread, so nothing else writes here.
    const size_t seen = g_probe.connects.load(std::memory_order_relaxed);
    if (seen == 0) {
      g_probe.bytes_at_first_connect = t_bytes;
    } else if (seen == 1) {
      g_probe.bytes_at_second_connect = t_bytes;
    }
    g_probe.connects.fetch_add(1, std::memory_order_release);
  });
  ASSERT_TRUE(server->start_sync());

  // Two connections, measured between them: the delta covers exactly one
  // accept-and-construct cycle with io_context and acceptor startup - which
  // legitimately allocates - already excluded by the first connection.
  std::vector<std::shared_ptr<wirestead::TcpClient>> clients;
  for (int i = 0; i < 2; ++i) {
    auto client = wirestead::tcp_client("127.0.0.1", port).max_retries(50).build();
    ASSERT_TRUE(client->start_sync()) << "client " << i << " never connected";
    clients.push_back(std::move(client));

    // start_sync() only promises the client's half of the handshake; the
    // server registers the session on its own io thread. Wait for the accept
    // to land so the two measurements bracket one connection, not two. Acquire
    // pairs with the release in the callback: observing the count here also
    // makes that connection's byte snapshot visible.
    ASSERT_TRUE(TestUtils::waitForCondition(
        [&] { return g_probe.connects.load(std::memory_order_acquire) > static_cast<size_t>(i); }, 5000))
        << "server never accepted connection " << i;
  }

  const size_t per_accept = g_probe.bytes_at_second_connect - g_probe.bytes_at_first_connect;

  // The prefill this gates against is 1,044,480 bytes on its own
  // (12 buffers each of 1 KiB, 4 KiB, 16 KiB and 64 KiB). What a session
  // legitimately costs is dominated by the 4 KiB default read buffer plus the
  // session object, its deques and asio's per-connection state - tens of KiB,
  // not hundreds.
  //
  // 256 KiB sits an order of magnitude below the regression and an order of
  // magnitude above the real cost, so it neither flakes on allocator or
  // platform differences nor goes blind. If a legitimate change pushes a
  // session past this, re-measure rather than raising the bound: a session
  // that grew to a quarter megabyte is itself the finding.
  constexpr size_t kMaxBytesPerAccept = 256 * 1024;

  // An upper bound alone would still pass if the counter never saw the io
  // thread's allocations at all - the bound would read zero and hold. A session
  // always costs something (the 4 KiB read buffer alone), so zero means the
  // operator new replacement above is not interposing on the library: that
  // happens when the tests link wirestead_shared instead of wirestead_static
  // (-DWIRESTEAD_BUILD_STATIC=OFF), because a Windows DLL keeps its own
  // operator new. Fail there rather than pass while measuring nothing.
  EXPECT_GT(per_accept, 0U) << "no allocation was counted for an accepted session - the global operator new "
                               "replacement is not interposing on the library, so this test is measuring nothing";

  EXPECT_LT(per_accept, kMaxBytesPerAccept)
      << "accepting a connection allocated " << per_accept
      << " bytes on the server io thread - something on the accept path is eagerly allocating per session again";

  for (auto& client : clients) client->stop();
  server->stop();
}
