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

#include <functional>

#include "unilink/common/platform.hpp"

#include <boost/asio.hpp>

namespace unilink {
namespace interface {

namespace net = boost::asio;

/**
 * @brief An interface abstracting Boost.Asio's steady_timer for testability.
 * This is an internal interface used for dependency injection and mocking.
 */
class TimerInterface {
 public:
  virtual ~TimerInterface() = default;

  virtual void expires_after(std::chrono::milliseconds expiry_time) = 0;
  virtual void async_wait(std::function<void(const boost::system::error_code&)> handler) = 0;
  virtual void cancel() = 0;
};

}  // namespace interface
}  // namespace unilink
