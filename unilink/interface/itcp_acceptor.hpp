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

#include <boost/asio.hpp>
#include <functional>

#include "unilink/common/platform.hpp"

namespace unilink {
namespace interface {

namespace net = boost::asio;

/**
 * @brief An interface abstracting Boost.Asio's tcp::acceptor for testability.
 * This is an internal interface used for dependency injection and mocking.
 */
class TcpAcceptorInterface {
 public:
  virtual ~TcpAcceptorInterface() = default;

  virtual void open(const net::ip::tcp& protocol, boost::system::error_code& ec) = 0;
  virtual void bind(const net::ip::tcp::endpoint& endpoint, boost::system::error_code& ec) = 0;
  virtual void listen(int backlog, boost::system::error_code& ec) = 0;
  virtual bool is_open() const = 0;
  virtual void close(boost::system::error_code& ec) = 0;

  virtual void async_accept(std::function<void(const boost::system::error_code&, net::ip::tcp::socket)> handler) = 0;
};

}  // namespace interface
}  // namespace unilink
