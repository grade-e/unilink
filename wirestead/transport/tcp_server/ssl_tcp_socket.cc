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

#include "wirestead/transport/tcp_server/ssl_tcp_socket.hpp"

#ifdef WIRESTEAD_TLS_ENABLED

#include <utility>

namespace wirestead {
namespace transport {

SslTcpSocket::SslTcpSocket(tcp::socket sock, std::shared_ptr<ssl::context> context)
    : context_(std::move(context)), stream_(std::move(sock), *context_) {}

void SslTcpSocket::async_read_some(const net::mutable_buffer& buffer,
                                   std::function<void(const boost::system::error_code&, std::size_t)> handler) {
  stream_.async_read_some(buffer, std::move(handler));
}

void SslTcpSocket::async_write(const net::const_buffer& buffer,
                               std::function<void(const boost::system::error_code&, std::size_t)> handler) {
  net::async_write(stream_, buffer, std::move(handler));
}

void SslTcpSocket::async_write(const std::vector<net::const_buffer>& buffers,
                               std::function<void(const boost::system::error_code&, std::size_t)> handler) {
  net::async_write(stream_, buffers, std::move(handler));
}

void SslTcpSocket::async_handshake(std::function<void(const boost::system::error_code&)> handler) {
  stream_.async_handshake(ssl::stream_base::server,
                          [handler = std::move(handler)](const boost::system::error_code& ec) {
                            if (handler) handler(ec);
                          });
}

// TLS wants close_notify sent before the transport goes away, but the caller
// reaches here through the plain-socket shutdown() contract, which is
// synchronous and cannot wait for the peer's reply. Send our half and drop the
// connection: an unclean shutdown is visible to the peer as a truncated stream,
// which is the same thing a TCP RST would tell it, and waiting here would block
// the io thread on a peer that may never answer.
void SslTcpSocket::shutdown(tcp::socket::shutdown_type what, boost::system::error_code& ec) {
  boost::system::error_code ignored;
  stream_.shutdown(ignored);
  stream_.lowest_layer().shutdown(what, ec);
}

void SslTcpSocket::close(boost::system::error_code& ec) { stream_.lowest_layer().close(ec); }

tcp::endpoint SslTcpSocket::remote_endpoint(boost::system::error_code& ec) const {
  return stream_.lowest_layer().remote_endpoint(ec);
}

}  // namespace transport
}  // namespace wirestead

#endif  // WIRESTEAD_TLS_ENABLED
