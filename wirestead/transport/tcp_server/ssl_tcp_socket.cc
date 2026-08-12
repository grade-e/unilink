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

#include <openssl/ssl.h>

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

// Sends close_notify without waiting for the peer's.
//
// ssl::stream::shutdown() is the obvious call here and is wrong: it runs the
// full bidirectional exchange and blocks until the peer answers. Measured
// against two peers that completed the handshake and then stopped reading,
// stop() went from 0 ms to 8035 ms - an io thread parked on a peer under no
// obligation to reply, for as long as it feels like not replying.
//
// SSL_shutdown's first call writes our close_notify and returns 0 rather than
// waiting for the reply; only a second call would block for it. One call is
// exactly the half we want. The peer sees a clean end of stream instead of a
// truncation, and a peer that never answers costs us nothing.
void SslTcpSocket::shutdown(tcp::socket::shutdown_type what, boost::system::error_code& ec) {
  ::SSL_shutdown(stream_.native_handle());
  stream_.lowest_layer().shutdown(what, ec);
}

void SslTcpSocket::close(boost::system::error_code& ec) { stream_.lowest_layer().close(ec); }

tcp::endpoint SslTcpSocket::remote_endpoint(boost::system::error_code& ec) const {
  return stream_.lowest_layer().remote_endpoint(ec);
}

}  // namespace transport
}  // namespace wirestead

#endif  // WIRESTEAD_TLS_ENABLED
