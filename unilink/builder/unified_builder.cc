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

#include "unilink/builder/unified_builder.hpp"

namespace unilink {
namespace builder {

TcpServerBuilder UnifiedBuilder::tcp_server(uint16_t port) { return TcpServerBuilder(port); }

TcpClientBuilder UnifiedBuilder::tcp_client(const std::string& host, uint16_t port) {
  return TcpClientBuilder(host, port);
}

SerialBuilder UnifiedBuilder::serial(const std::string& device, uint32_t baud_rate) {
  return SerialBuilder(device, baud_rate);
}

}  // namespace builder
}  // namespace unilink
