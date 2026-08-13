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

#ifdef WIRESTEAD_TLS_ENABLED

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <functional>
#include <memory>
#include <vector>

#include "wirestead/base/visibility.hpp"
#include "wirestead/interface/itcp_socket.hpp"

namespace wirestead {
namespace transport {

namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

/**
 * @brief TLS implementation of TcpSocketInterface for accepted server sockets.
 *
 * The session above this only ever reads, writes, handshakes, shuts down and
 * closes, so wrapping ssl::stream is enough to give it TLS without the session
 * knowing. Only built when WIRESTEAD_ENABLE_TLS is on.
 */
class WIRESTEAD_API SslTcpSocket : public interface::TcpSocketInterface {
 public:
  // The context is shared across every accepted connection and must outlive
  // them, which is why it arrives as a shared_ptr rather than a reference.
  SslTcpSocket(tcp::socket sock, std::shared_ptr<ssl::context> context);
  ~SslTcpSocket() override = default;

  void async_read_some(const net::mutable_buffer& buffer,
                       std::function<void(const boost::system::error_code&, std::size_t)> handler) override;
  void async_write(const net::const_buffer& buffer,
                   std::function<void(const boost::system::error_code&, std::size_t)> handler) override;

  // ssl::stream has no scatter-gather write of its own - the record layer has
  // to see one contiguous plaintext run anyway - so this hands the sequence to
  // net::async_write, which coalesces it into as few TLS records as it can.
  // The syscall-per-message saving from #572 survives; the writev does not.
  void async_write(const std::vector<net::const_buffer>& buffers,
                   std::function<void(const boost::system::error_code&, std::size_t)> handler) override;

  void shutdown(tcp::socket::shutdown_type what, boost::system::error_code& ec) override;
  void close(boost::system::error_code& ec) override;
  tcp::endpoint remote_endpoint(boost::system::error_code& ec) const override;
  void async_handshake(std::function<void(const boost::system::error_code&)> handler) override;

 private:
  // Held so the context cannot be destroyed while a connection still uses it.
  std::shared_ptr<ssl::context> context_;
  ssl::stream<tcp::socket> stream_;
};

}  // namespace transport
}  // namespace wirestead

#endif  // WIRESTEAD_TLS_ENABLED
