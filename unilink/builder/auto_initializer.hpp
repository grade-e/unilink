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

#if __has_include(<unilink_export.hpp>)
#include <unilink_export.hpp>
#else
#if defined(_WIN32) || defined(__CYGWIN__)
#  ifdef UNILINK_EXPORTS
#    define UNILINK_EXPORT __declspec(dllexport)
#  else
#    define UNILINK_EXPORT __declspec(dllimport)
#  endif
#else
#  define UNILINK_EXPORT
#endif
#endif

#include <mutex>

#include "unilink/common/io_context_manager.hpp"

namespace unilink {
namespace builder {

/**
 * @brief Helper class that automatically initializes IoContextManager in Builder pattern
 *
 * This class automatically starts IoContextManager when using Builder pattern,
 * eliminating the need for manual initialization by users.
 */
class UNILINK_EXPORT AutoInitializer {
 public:
  /**
   * @brief Automatically start IoContextManager if not running
   *
   * This method is thread-safe and can be called multiple times safely.
   * If already running, it does nothing.
  */
  static void ensure_io_context_running() {
    if (!common::IoContextManager::instance().is_running()) {
      std::lock_guard<std::mutex> lock(init_mutex());
      // Double-check locking
      if (!common::IoContextManager::instance().is_running()) {
        common::IoContextManager::instance().start();
      }
    }
  }

  /**
   * @brief Check if IoContextManager is running
  */
  static bool is_io_context_running() { return common::IoContextManager::instance().is_running(); }

 private:
  static std::mutex& init_mutex();
};

}  // namespace builder
}  // namespace unilink
