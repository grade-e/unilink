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

#include "wirestead/base/constants.hpp"
#include "wirestead/config/tcp_client_config.hpp"
#include "wirestead/config/tcp_server_config.hpp"
#include "wirestead/config/uds_config.hpp"

using namespace wirestead;
using namespace wirestead::config;
namespace k = wirestead::base::constants;

namespace {

// Every config carrying a read buffer clamps it the same way, so run the same
// checks over each rather than letting one drift.
template <typename Config>
void expect_read_buffer_clamped() {
  Config cfg;
  EXPECT_EQ(cfg.read_buffer_size, k::DEFAULT_READ_BUFFER_SIZE) << "default must not change silently";

  cfg.read_buffer_size = 1;
  EXPECT_FALSE(cfg.is_valid());
  cfg.validate_and_clamp();
  EXPECT_EQ(cfg.read_buffer_size, k::MIN_READ_BUFFER_SIZE);
  EXPECT_TRUE(cfg.is_valid());

  cfg.read_buffer_size = k::MAX_READ_BUFFER_SIZE * 4;
  EXPECT_FALSE(cfg.is_valid());
  cfg.validate_and_clamp();
  EXPECT_EQ(cfg.read_buffer_size, k::MAX_READ_BUFFER_SIZE);
  EXPECT_TRUE(cfg.is_valid());

  // A value already in range must survive untouched.
  cfg.read_buffer_size = 32 * 1024;
  cfg.validate_and_clamp();
  EXPECT_EQ(cfg.read_buffer_size, 32u * 1024u);
}

}  // namespace

TEST(ReadBufferSizeConfigTest, TcpClientConfigClampsToRange) { expect_read_buffer_clamped<TcpClientConfig>(); }

TEST(ReadBufferSizeConfigTest, TcpServerConfigClampsToRange) { expect_read_buffer_clamped<TcpServerConfig>(); }

TEST(ReadBufferSizeConfigTest, UdsClientConfigClampsToRange) { expect_read_buffer_clamped<UdsClientConfig>(); }

TEST(ReadBufferSizeConfigTest, UdsServerConfigClampsToRange) { expect_read_buffer_clamped<UdsServerConfig>(); }

TEST(ReadBufferSizeConfigTest, BoundsAreOrdered) {
  EXPECT_LT(k::MIN_READ_BUFFER_SIZE, k::MAX_READ_BUFFER_SIZE);
  EXPECT_GE(k::DEFAULT_READ_BUFFER_SIZE, k::MIN_READ_BUFFER_SIZE);
  EXPECT_LE(k::DEFAULT_READ_BUFFER_SIZE, k::MAX_READ_BUFFER_SIZE);
  // The read buffer is per connection and a server multiplies it by
  // max_connections, so it must stay well under the kernel socket-buffer cap.
  EXPECT_LT(k::MAX_READ_BUFFER_SIZE, k::MAX_SOCKET_BUFFER_SIZE);
}
