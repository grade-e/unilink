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
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "test_utils.hpp"
#include "wirestead/wirestead.hpp"

using namespace wirestead;
using namespace wirestead::test;
using namespace std::chrono_literals;

namespace {

// Enough queued at once that do_write() gathers many buffers per write, which
// is the path these tests exist to pin down.
constexpr int kMessages = 2000;

}  // namespace

// A gather write must put exactly the queued bytes on the wire - no more, no
// less. An earlier implementation handed asio a non-owning view of the buffer
// sequence; asio's partial-write bookkeeping did not survive that, and queued
// buffers were silently duplicated on the wire or left stuck in the queue.
// Byte counts alone catch both, so assert them directly.
TEST(GatherWriteTest, SendsExactlyWhatWasAccepted) {
  const uint16_t port = TestUtils::getAvailableTestPort();
  std::atomic<size_t> received{0};

  auto server = wirestead::tcp_server(port).build();
  server->on_data([&](const wirestead::MessageContext& ctx) { received.fetch_add(ctx.data().size()); });
  ASSERT_TRUE(server->start_sync());

  auto client = wirestead::tcp_client("127.0.0.1", port).max_retries(50).build();
  ASSERT_TRUE(client->start_sync());

  const std::string payload(1024, 'x');
  size_t accepted_by_app = 0;
  for (int i = 0; i < kMessages; ++i) {
    if (client->try_send(payload)) accepted_by_app += payload.size();
  }
  ASSERT_GT(accepted_by_app, 0u);

  ASSERT_TRUE(TestUtils::waitForCondition([&] { return received.load() >= accepted_by_app; }, 10000))
      << "only " << received.load() << " of " << accepted_by_app << " bytes arrived - queued buffers were lost";

  const auto stats = client->stats();
  EXPECT_EQ(stats.bytes_sent, stats.bytes_accepted) << "bytes on the wire must match what the queue accepted";
  EXPECT_EQ(received.load(), accepted_by_app) << "receiver must see exactly the accepted bytes, never a duplicate";

  client->stop();
  server->stop();
}

// The same guarantee for the ownership-transfer path, whose queued buffers are
// plain vectors rather than pooled buffers - a different variant alternative
// feeding the same gather.
TEST(GatherWriteTest, SendMovePreservesByteCountAndOrder) {
  const uint16_t port = TestUtils::getAvailableTestPort();
  std::mutex mtx;
  std::vector<uint8_t> assembled;

  auto server = wirestead::tcp_server(port).build();
  server->on_data([&](const wirestead::MessageContext& ctx) {
    const auto bytes = ctx.data_as_vector();
    std::lock_guard<std::mutex> lock(mtx);
    assembled.insert(assembled.end(), bytes.begin(), bytes.end());
  });
  ASSERT_TRUE(server->start_sync());

  auto client = wirestead::tcp_client("127.0.0.1", port).max_retries(50).build();
  ASSERT_TRUE(client->start_sync());

  // Each message is a distinct byte value, so a duplicated or reordered
  // buffer shows up as a mismatch rather than just a wrong total.
  std::vector<uint8_t> expected;
  int sent_messages = 0;
  for (int i = 0; i < 512; ++i) {
    std::vector<uint8_t> msg(256, static_cast<uint8_t>(i % 256));
    auto copy = msg;
    if (client->send_move(std::move(msg))) {
      expected.insert(expected.end(), copy.begin(), copy.end());
      ++sent_messages;
    }
  }
  ASSERT_GT(sent_messages, 0);

  ASSERT_TRUE(TestUtils::waitForCondition(
      [&] {
        std::lock_guard<std::mutex> lock(mtx);
        return assembled.size() >= expected.size();
      },
      10000));

  {
    std::lock_guard<std::mutex> lock(mtx);
    EXPECT_EQ(assembled.size(), expected.size());
    EXPECT_EQ(assembled, expected) << "gathered buffers must reach the peer in order, exactly once";
  }

  client->stop();
  server->stop();
}
