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
#include <memory>
#include <string>
#include <thread>

#include "test_utils.hpp"
#include "wirestead/wirestead.hpp"

using namespace wirestead;
using namespace wirestead::test;
using namespace std::chrono_literals;

class SerialBuilderValidationTest : public ::testing::Test {
 protected:
  void SetUp() override {
#ifdef _WIN32
    device_ = "NUL";
#else
    device_ = "/dev/null";
#endif
  }
  std::string device_;
};

TEST_F(SerialBuilderValidationTest, AcceptsValidatedSerialOptions) {
  auto serial_ptr = serial(device_, 115200)
                        .data_bits(8)
                        .stop_bits(1)
                        .parity("none")
                        .flow_control("none")
                        .on_data([](auto&&) {})
                        .on_error([](auto&&) {})
                        .build();

  ASSERT_NE(serial_ptr, nullptr);
}

TEST_F(SerialBuilderValidationTest, AcceptsRetryIntervalOption) {
  auto serial_ptr = serial(device_, 9600).retry_interval(500ms).on_data([](auto&&) {}).on_error([](auto&&) {}).build();
  ASSERT_NE(serial_ptr, nullptr);
}

TEST_F(SerialBuilderValidationTest, InvalidParityFallsBackDuringBuild) {
  EXPECT_NO_THROW({
    auto serial_ptr = serial(device_, 9600).parity("invalid").on_data([](auto&&) {}).on_error([](auto&&) {}).build();
    ASSERT_NE(serial_ptr, nullptr);
  });
}

// read_chunk reached the config layer but had no builder or wrapper surface,
// so serial was the one transport whose read buffer could not be set the way
// TCP and UDS set read_buffer_size(). These pin the surface down; the value
// itself is exercised at the config layer, which is where the clamp lives.
TEST_F(SerialBuilderValidationTest, AcceptsReadChunkOption) {
  auto serial_ptr = serial(device_, 115200).read_chunk(16384).on_data([](auto&&) {}).on_error([](auto&&) {}).build();
  ASSERT_NE(serial_ptr, nullptr);
}

TEST_F(SerialBuilderValidationTest, OutOfRangeReadChunkIsClampedNotRejected) {
  EXPECT_NO_THROW({
    auto too_small = serial(device_, 9600).read_chunk(1).on_data([](auto&&) {}).on_error([](auto&&) {}).build();
    ASSERT_NE(too_small, nullptr);
    auto too_large =
        serial(device_, 9600).read_chunk(64 * 1024 * 1024).on_data([](auto&&) {}).on_error([](auto&&) {}).build();
    ASSERT_NE(too_large, nullptr);
  });
}
