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

// The backpressure setters are spread across five different storage shapes:
// a plain member, a std::atomic member, and a field on an embedded config
// struct, guarded by either the impl mutex or by the atomic itself. Each getter
// has to match the shape its own class uses, so every transport is covered
// here rather than one representative.

#include <gtest/gtest.h>

#include "wirestead/base/constants.hpp"
#include "wirestead/config/udp_config.hpp"
#include "wirestead/wrapper/serial/serial.hpp"
#include "wirestead/wrapper/tcp_client/tcp_client.hpp"
#include "wirestead/wrapper/tcp_server/tcp_server.hpp"
#include "wirestead/wrapper/udp/udp.hpp"
#include "wirestead/wrapper/udp/udp_server.hpp"
#include "wirestead/wrapper/uds_client/uds_client.hpp"
#include "wirestead/wrapper/uds_server/uds_server.hpp"

using namespace wirestead;
using base::constants::BackpressureStrategy;
using base::constants::DEFAULT_BACKPRESSURE_THRESHOLD;

namespace wirestead {
namespace test {
namespace {

// Reads back what was set, and reports the documented default before any set.
template <typename T>
void CheckRoundTrip(T& transport) {
  EXPECT_EQ(transport.backpressure_threshold(), DEFAULT_BACKPRESSURE_THRESHOLD);
  EXPECT_EQ(transport.backpressure_strategy(), BackpressureStrategy::Reliable);

  transport.backpressure_threshold(4096);
  EXPECT_EQ(transport.backpressure_threshold(), 4096u);

  transport.backpressure_strategy(BackpressureStrategy::BestEffort);
  EXPECT_EQ(transport.backpressure_strategy(), BackpressureStrategy::BestEffort);

  // Setters chain, so the getter must survive a chained call.
  transport.backpressure_threshold(8192).backpressure_strategy(BackpressureStrategy::Reliable);
  EXPECT_EQ(transport.backpressure_threshold(), 8192u);
  EXPECT_EQ(transport.backpressure_strategy(), BackpressureStrategy::Reliable);
}

}  // namespace

TEST(BackpressureGettersTest, Serial) {
  wrapper::Serial s("/dev/null", 9600);
  CheckRoundTrip(s);
}

TEST(BackpressureGettersTest, TcpClient) {
  wrapper::TcpClient c("127.0.0.1", 9000);
  CheckRoundTrip(c);
}

TEST(BackpressureGettersTest, TcpServer) {
  wrapper::TcpServer s(0);
  CheckRoundTrip(s);
}

TEST(BackpressureGettersTest, UdpClient) {
  config::UdpConfig cfg;
  cfg.remote_address = "127.0.0.1";
  cfg.remote_port = 9000;
  wrapper::UdpClient c(cfg);
  CheckRoundTrip(c);
}

TEST(BackpressureGettersTest, UdpServer) {
  wrapper::UdpServer s(0);
  CheckRoundTrip(s);
}

TEST(BackpressureGettersTest, UdsClient) {
  wrapper::UdsClient c("/tmp/wirestead-backpressure-getters.sock");
  CheckRoundTrip(c);
}

TEST(BackpressureGettersTest, UdsServer) {
  wrapper::UdsServer s("/tmp/wirestead-backpressure-getters.sock");
  CheckRoundTrip(s);
}

}  // namespace test
}  // namespace wirestead
