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

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "unilink/common/platform.hpp"

#include <boost/asio.hpp>

namespace unilink {
namespace common {

/**
 * Global io_context manager
 * All Transports share one io_context for improved memory efficiency
 * Added independent context creation functionality for test isolation
 */
class IoContextManager {
 public:
  using IoContext = boost::asio::io_context;
  using WorkGuard = boost::asio::executor_work_guard<IoContext::executor_type>;

  // Singleton instance access
  static IoContextManager& instance();

  // Return io_context reference (existing functionality)
  IoContext& get_context();

  // Start/stop io_context (existing functionality)
  void start();
  void stop();

  // Check status (existing functionality)
  bool is_running() const;

  // 🆕 Create independent io_context (for test isolation)
  std::unique_ptr<IoContext> create_independent_context();

  // Automatic cleanup in destructor
  ~IoContextManager();

 private:
  IoContextManager() = default;
  IoContextManager(const IoContextManager&) = delete;
  IoContextManager& operator=(const IoContextManager&) = delete;

  std::unique_ptr<IoContext> ioc_;
  std::unique_ptr<WorkGuard> work_guard_;
  std::thread io_thread_;
  std::atomic<bool> running_{false};
  mutable std::mutex mutex_;
};

}  // namespace common
}  // namespace unilink
