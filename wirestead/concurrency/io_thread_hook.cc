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

#include "wirestead/concurrency/io_thread_hook.hpp"

#include <memory>
#include <mutex>

namespace wirestead {
namespace concurrency {

namespace {

std::mutex& hook_mutex() {
  static std::mutex m;
  return m;
}

// shared_ptr to const, so a thread that has taken a snapshot keeps a valid
// callable even while another thread installs a replacement - the same
// discipline interface::SharedCallback uses on the receive path.
std::shared_ptr<const std::function<void()>>& hook_storage() {
  static std::shared_ptr<const std::function<void()>> hook;
  return hook;
}

}  // namespace

void set_io_thread_init(std::function<void()> fn) {
  auto installed = fn ? std::make_shared<const std::function<void()>>(std::move(fn))
                      : std::shared_ptr<const std::function<void()>>{};
  std::lock_guard<std::mutex> lock(hook_mutex());
  hook_storage() = std::move(installed);
}

void run_io_thread_init() noexcept {
  std::shared_ptr<const std::function<void()>> hook;
  {
    std::lock_guard<std::mutex> lock(hook_mutex());
    hook = hook_storage();
  }
  if (!hook) return;
  // An exception here would cross the thread entry point and terminate the
  // process. Whatever the hook was setting is not worth that, and the thread
  // still has io to service.
  try {
    (*hook)();
  } catch (...) {
  }
}

}  // namespace concurrency
}  // namespace wirestead
