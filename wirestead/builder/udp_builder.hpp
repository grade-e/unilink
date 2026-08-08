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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "wirestead/base/visibility.hpp"
#include "wirestead/builder/ibuilder.hpp"
#include "wirestead/wrapper/udp/udp.hpp"
#include "wirestead/wrapper/udp/udp_server.hpp"

namespace wirestead {
namespace builder {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Modernized Builder for UdpClient
 */
class WIRESTEAD_API UdpClientBuilder : public BuilderInterface<wrapper::UdpClient, UdpClientBuilder> {
 public:
  UdpClientBuilder();
  explicit UdpClientBuilder(uint16_t local_port);

  // Delete copy
  UdpClientBuilder(const UdpClientBuilder&) = delete;
  UdpClientBuilder& operator=(const UdpClientBuilder&) = delete;

  std::unique_ptr<wrapper::UdpClient> build() override;

  UdpClientBuilder& auto_start(bool auto_start = true) override;
  UdpClientBuilder& local_port(uint16_t port);
  UdpClientBuilder& bind_address(const std::string& address);
  [[deprecated("Use bind_address instead")]]
  UdpClientBuilder& local_address(const std::string& address) {
    return bind_address(address);
  }
  UdpClientBuilder& remote_endpoint(const std::string& host, uint16_t port);
  UdpClientBuilder& remote(const std::string& host, uint16_t port) { return remote_endpoint(host, port); }
  UdpClientBuilder& broadcast(bool enable = true);
  UdpClientBuilder& reuse_address(bool enable = true);
  UdpClientBuilder& independent_context(bool use_independent = true);
  UdpClientBuilder& send_buffer_size(size_t bytes);
  UdpClientBuilder& receive_buffer_size(size_t bytes);

 private:
  uint16_t local_port_;
  std::string bind_address_;
  std::string remote_host_;
  uint16_t remote_port_;
  bool auto_start_;
  bool independent_context_;
  bool enable_broadcast_;
  bool reuse_address_;
  size_t send_buffer_size_;
  size_t receive_buffer_size_;
};

using UdpClientBuilderDefault = UdpClientBuilder;

/**
 * @brief Modernized Builder for UdpServer
 */
class WIRESTEAD_API UdpServerBuilder : public BuilderInterface<wrapper::UdpServer, UdpServerBuilder> {
 public:
  UdpServerBuilder();
  explicit UdpServerBuilder(uint16_t local_port);

  // Delete copy
  UdpServerBuilder(const UdpServerBuilder&) = delete;
  UdpServerBuilder& operator=(const UdpServerBuilder&) = delete;

  std::unique_ptr<wrapper::UdpServer> build() override;

  UdpServerBuilder& auto_start(bool auto_start = true) override;
  UdpServerBuilder& local_port(uint16_t port);
  UdpServerBuilder& bind_address(const std::string& address);
  [[deprecated("Use bind_address instead")]]
  UdpServerBuilder& local_address(const std::string& address) {
    return bind_address(address);
  }
  UdpServerBuilder& max_clients(uint32_t max);
  UdpServerBuilder& broadcast(bool enable = true);
  UdpServerBuilder& reuse_address(bool enable = true);
  UdpServerBuilder& independent_context(bool use_independent = true);
  /**
   * @brief Configure application-level idle timeout for virtual sessions.
   *
   * A value of 0ms disables idle timeout. When enabled, stale UDP virtual
   * sessions are removed and a later datagram from the same endpoint creates a
   * new virtual session.
   */
  UdpServerBuilder& idle_timeout(std::chrono::milliseconds timeout);
  UdpServerBuilder& send_buffer_size(size_t bytes);
  UdpServerBuilder& receive_buffer_size(size_t bytes);

 private:
  uint16_t local_port_;
  std::string bind_address_;
  bool auto_start_;
  bool independent_context_;
  bool enable_broadcast_;
  bool reuse_address_;
  uint32_t max_clients_ = 0;
  bool client_limit_enabled_ = false;
  std::chrono::milliseconds idle_timeout_{0};
  bool idle_timeout_set_ = false;
  size_t send_buffer_size_;
  size_t receive_buffer_size_;
};

using UdpServerBuilderDefault = UdpServerBuilder;

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace builder
}  // namespace wirestead
