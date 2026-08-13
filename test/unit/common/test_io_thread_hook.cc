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
#include <future>
#include <thread>

#include "wirestead/builder/tcp_client_builder.hpp"
#include "wirestead/concurrency/io_thread_hook.hpp"

using namespace std::chrono_literals;

namespace {

// Every test here installs a hook, so none may leave one behind for the next.
class IoThreadHookTest : public ::testing::Test {
 protected:
  void TearDown() override { wirestead::concurrency::set_io_thread_init(nullptr); }
};

}  // namespace

// The hook exists so a deployment can call pthread_setschedparam(pthread_self())
// and friends, which only work from the thread being configured. Running on the
// caller's thread instead would configure the wrong one and look like it
// worked, so the thread identity is the assertion.
TEST_F(IoThreadHookTest, RunsOnTheIoThreadNotTheCaller) {
  std::promise<std::thread::id> ran_on;
  std::atomic<bool> fired{false};
  wirestead::concurrency::set_io_thread_init([&] {
    if (!fired.exchange(true)) ran_on.set_value(std::this_thread::get_id());
  });

  auto client = wirestead::builder::TcpClientBuilder("127.0.0.1", 59999).build();
  auto started = client->start();

  auto future = ran_on.get_future();
  ASSERT_EQ(future.wait_for(2s), std::future_status::ready) << "the hook never ran on any io thread";
  EXPECT_NE(future.get(), std::this_thread::get_id());

  client->stop();
  started.wait_for(2s);
}

TEST_F(IoThreadHookTest, NoHookInstalledIsFine) {
  wirestead::concurrency::set_io_thread_init(nullptr);

  auto client = wirestead::builder::TcpClientBuilder("127.0.0.1", 59999).build();
  auto started = client->start();
  std::this_thread::sleep_for(100ms);
  EXPECT_NO_THROW(client->stop());
  started.wait_for(2s);
}

// A throwing hook reaches the thread entry point, where an escaping exception
// terminates the process. The io thread has work to do either way.
TEST_F(IoThreadHookTest, AThrowingHookDoesNotTakeDownTheThread) {
  std::atomic<int> calls{0};
  wirestead::concurrency::set_io_thread_init([&] {
    calls.fetch_add(1);
    throw std::runtime_error("hook blew up");
  });

  auto client = wirestead::builder::TcpClientBuilder("127.0.0.1", 59999).build();
  auto started = client->start();
  std::this_thread::sleep_for(200ms);

  EXPECT_GE(calls.load(), 1);
  EXPECT_NO_THROW(client->stop());
  started.wait_for(2s);
}
