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
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "test_utils.hpp"
#include "wirestead/builder/auto_initializer.hpp"
#include "wirestead/wirestead.hpp"

using namespace wirestead;
using namespace wirestead::test;
using namespace std::chrono_literals;

class UdsIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto temp_path = TestUtils::makeUniqueUdsSocketPath("ulit");
    socket_path_ = temp_path.string();
    TestUtils::removeFileIfExists(temp_path);
  }

  void TearDown() override { TestUtils::removeFileIfExists(socket_path_); }

  std::string socket_path_;
};

TEST_F(UdsIntegrationTest, BuilderPatternIntegration) {
  auto server = wirestead::uds_server(socket_path_).on_data([](auto&&) {}).on_error([](auto&&) {}).build();
  EXPECT_NE(server, nullptr);

  auto client = wirestead::uds_client(socket_path_).on_data([](auto&&) {}).on_error([](auto&&) {}).build();
  EXPECT_NE(client, nullptr);
}

TEST_F(UdsIntegrationTest, BasicCommunication) {
  std::atomic<bool> server_connected{false};
  std::atomic<bool> client_connected{false};
  std::atomic<bool> data_received{false};
  std::string received_data;
  std::mutex mtx;
  std::condition_variable cv;

  auto server = wirestead::uds_server(socket_path_)
                    .independent_context(true)
                    .on_connect([&server_connected](const wrapper::ConnectionContext&) { server_connected = true; })
                    .on_data([&](const wrapper::MessageContext& ctx) {
                      std::lock_guard<std::mutex> lock(mtx);
                      received_data = std::string(ctx.data());
                      data_received = true;
                      cv.notify_one();
                    })
                    .on_error([](auto&&) {})
                    .build();

  server->start();

  // Wait for server to be ready
  bool listening = TestUtils::waitForCondition([&]() { return server->listening(); }, 2000);
  ASSERT_TRUE(listening) << "Server failed to start listening";

  auto client = wirestead::uds_client(socket_path_)
                    .independent_context(true)
                    .on_connect([&client_connected](const wrapper::ConnectionContext&) { client_connected = true; })
                    .on_data([](auto&&) {})
                    .on_error([](auto&&) {})
                    .build();

  client->start();

  // Wait for connection
  bool connected = TestUtils::waitForCondition([&]() { return server_connected && client_connected; }, 5000);
  ASSERT_TRUE(connected) << "Failed to establish connection";

  // Send data
  std::string test_msg = "Hello UDS!";
  client->send(test_msg);

  // Wait for data
  std::unique_lock<std::mutex> lock(mtx);
  bool success = cv.wait_for(lock, 5s, [&]() { return data_received.load(); });

  EXPECT_TRUE(success) << "Data was not received by server";
  EXPECT_EQ(received_data, test_msg);

  client->stop();
  server->stop();
}

// use_length_prefix_framer() exists because the pattern-based framers cannot
// carry a binary payload: PacketFramer ends the frame at the first occurrence
// of its end pattern wherever it falls, and LineFramer at the first newline.
// The payload here contains 0x02, 0x03 and '\n' for exactly that reason, so
// this fails if build() ever stops installing the framer the builder was
// asked for and falls back to a delimiter-based one.
TEST_F(UdsIntegrationTest, LengthPrefixFramerCarriesAnArbitraryBinaryPayload) {
  const std::vector<uint8_t> payload = {0x02, 0x00, 0x03, '\n', 0xFF, 'a', 0x00, 0x03};

  std::mutex mtx;
  std::condition_variable cv;
  std::vector<uint8_t> received;
  std::atomic<bool> message_received{false};
  std::atomic<bool> server_connected{false};

  auto server = wirestead::uds_server(socket_path_)
                    .independent_context(true)
                    .use_length_prefix_framer(2)
                    .on_connect([&server_connected](const wrapper::ConnectionContext&) { server_connected = true; })
                    .on_message([&](const wrapper::MessageContext& ctx) {
                      std::lock_guard<std::mutex> lock(mtx);
                      received = ctx.data_as_vector();
                      message_received = true;
                      cv.notify_one();
                    })
                    .on_error([](auto&&) {})
                    .build();

  server->start();
  ASSERT_TRUE(TestUtils::waitForCondition([&]() { return server->listening(); }, 2000))
      << "Server failed to start listening";

  auto client = wirestead::uds_client(socket_path_)
                    .independent_context(true)
                    .on_data([](auto&&) {})
                    .on_error([](auto&&) {})
                    .build();

  client->start();
  ASSERT_TRUE(TestUtils::waitForCondition([&]() { return server_connected.load(); }, 5000))
      << "Failed to establish connection";

  // 2-byte big-endian length, excluding the prefix itself.
  std::vector<uint8_t> frame = {static_cast<uint8_t>((payload.size() >> 8) & 0xFF),
                                static_cast<uint8_t>(payload.size() & 0xFF)};
  frame.insert(frame.end(), payload.begin(), payload.end());
  ASSERT_TRUE(client->send_move(std::move(frame)));

  std::unique_lock<std::mutex> lock(mtx);
  EXPECT_TRUE(cv.wait_for(lock, 5s, [&]() { return message_received.load(); })) << "Framed message never arrived";
  EXPECT_EQ(received, payload);
  lock.unlock();

  client->stop();
  server->stop();
}

TEST_F(UdsIntegrationTest, MultiClientCommunication) {
  std::atomic<int> connections{0};
  std::atomic<int> messages_received{0};
  std::mutex mtx;
  std::condition_variable cv;

  auto server = wirestead::uds_server(socket_path_)
                    .independent_context(true)
                    .on_connect([&connections](const wrapper::ConnectionContext&) { connections++; })
                    .on_data([&](const wrapper::MessageContext& ctx) { messages_received++; })
                    .on_error([](auto&&) {})
                    .build();

  server->start();
  TestUtils::waitForCondition([&]() { return server->listening(); }, 2000);

  auto client1 = wirestead::uds_client(socket_path_)
                     .independent_context(true)
                     .on_data([](auto&&) {})
                     .on_error([](auto&&) {})
                     .build();
  auto client2 = wirestead::uds_client(socket_path_)
                     .independent_context(true)
                     .on_data([](auto&&) {})
                     .on_error([](auto&&) {})
                     .build();

  client1->start();
  client2->start();

  bool connected = TestUtils::waitForCondition([&]() { return connections == 2; }, 5000);
  EXPECT_TRUE(connected);
  EXPECT_EQ(connections, 2);

  client1->send("Msg1");
  client2->send("Msg2");

  bool received = TestUtils::waitForCondition([&]() { return messages_received == 2; }, 5000);
  EXPECT_TRUE(received);
  EXPECT_EQ(messages_received, 2);

  client1->stop();
  client2->stop();
  server->stop();
}
