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

#include "wirestead/base/visibility.hpp"
#include "wirestead/builder/ibuilder.hpp"
#include "wirestead/wrapper/tcp_client/tcp_client.hpp"

namespace wirestead {
namespace builder {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Modernized Builder for TcpClient
 */
class WIRESTEAD_API TcpClientBuilder : public BuilderInterface<wrapper::TcpClient, TcpClientBuilder> {
 public:
  TcpClientBuilder(const std::string& host, uint16_t port);

  // Delete copy
  TcpClientBuilder(const TcpClientBuilder&) = delete;
  TcpClientBuilder& operator=(const TcpClientBuilder&) = delete;

  std::unique_ptr<wrapper::TcpClient> build() override;

  TcpClientBuilder& auto_start(bool auto_start = true) override;

  TcpClientBuilder& retry_interval(std::chrono::milliseconds interval);
  TcpClientBuilder& max_retries(int max_retries);
  /** @brief Connect over TLS, verifying the server. Empty ca_file uses the system trust store. */
  TcpClientBuilder& tls(const std::string& ca_file = "");

  TcpClientBuilder& connection_timeout(std::chrono::milliseconds timeout);
  /**
   * @brief Configure application-level idle timeout.
   *
   * A value of 0ms disables idle timeout. When enabled, inbound or outbound
   * activity resets the timer.
   */
  TcpClientBuilder& idle_timeout(std::chrono::milliseconds timeout);
  /**
   * @brief Configure what happens when an enabled idle timeout expires.
   *
   * The default is IdleTimeoutAction::Reconnect. This setting has no effect
   * while idle_timeout is 0ms.
   */
  TcpClientBuilder& idle_timeout_action(IdleTimeoutAction action);
  TcpClientBuilder& independent_context(bool use_independent = true);
  TcpClientBuilder& tcp_no_delay(bool enable = true);
  TcpClientBuilder& keep_alive(bool enable = true);
  TcpClientBuilder& send_buffer_size(size_t bytes);
  TcpClientBuilder& receive_buffer_size(size_t bytes);

  /**
   * @brief Size of the userspace buffer each read fills, in bytes.
   *
   * Raising this reduces read completions and callback dispatches on bulk
   * transfers, at the cost of that much memory per connection. Clamped to
   * [MIN_READ_BUFFER_SIZE, MAX_READ_BUFFER_SIZE].
   */
  TcpClientBuilder& read_buffer_size(size_t bytes);

 private:
  std::string host_;
  uint16_t port_;
  bool auto_start_;
  bool independent_context_;

  std::chrono::milliseconds retry_interval_;
  bool retry_interval_set_{false};
  int max_retries_;
  bool max_retries_set_{false};
  bool tls_enabled_{false};
  std::string tls_ca_file_;

  std::chrono::milliseconds connection_timeout_;
  bool connection_timeout_set_{false};
  std::chrono::milliseconds idle_timeout_;
  bool idle_timeout_set_{false};
  IdleTimeoutAction idle_timeout_action_;
  bool idle_timeout_action_set_{false};
  bool tcp_no_delay_;
  bool tcp_no_delay_set_{false};
  bool keep_alive_;
  bool keep_alive_set_{false};
  size_t send_buffer_size_;
  bool send_buffer_size_set_{false};
  size_t receive_buffer_size_;
  bool receive_buffer_size_set_{false};
  size_t read_buffer_size_;
  bool read_buffer_size_set_{false};
};

using TcpClientBuilderDefault = TcpClientBuilder;

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace builder
}  // namespace wirestead
