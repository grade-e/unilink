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
#include <memory>
#include <string>

#include "unilink/factory/channel_factory.hpp"
#include "unilink/wrapper/ichannel.hpp"

namespace unilink {
namespace wrapper {

/**
 * 개선된 TCP Server Wrapper
 * - 공유 io_context 사용
 * - 메모리 누수 방지
 * - 자동 리소스 관리
 */
class TcpServer : public ChannelInterface {
 public:
  explicit TcpServer(uint16_t port);
  explicit TcpServer(std::shared_ptr<interface::Channel> channel);
  ~TcpServer() = default;

  // IChannel 인터페이스 구현
  void start() override;
  void stop() override;
  void send(const std::string& data) override;
  bool is_connected() const override;

  ChannelInterface& on_data(DataHandler handler) override;
  ChannelInterface& on_connect(ConnectHandler handler) override;
  ChannelInterface& on_disconnect(DisconnectHandler handler) override;
  ChannelInterface& on_error(ErrorHandler handler) override;

  ChannelInterface& auto_start(bool start = true) override;
  ChannelInterface& auto_manage(bool manage = true) override;

  void send_line(const std::string& line) override;
  // void send_binary(const std::vector<uint8_t>& data) override;

  // 멀티 클라이언트 지원 메서드
  void broadcast(const std::string& message);
  void send_to_client(size_t client_id, const std::string& message);
  size_t get_client_count() const;
  std::vector<size_t> get_connected_clients() const;

  // 멀티 클라이언트 콜백 타입 정의
  using MultiClientConnectHandler = std::function<void(size_t client_id, const std::string& client_info)>;
  using MultiClientDataHandler = std::function<void(size_t client_id, const std::string& data)>;
  using MultiClientDisconnectHandler = std::function<void(size_t client_id)>;

  TcpServer& on_multi_connect(MultiClientConnectHandler handler);
  TcpServer& on_multi_data(MultiClientDataHandler handler);
  TcpServer& on_multi_disconnect(MultiClientDisconnectHandler handler);

  // 클라이언트 제한 설정
  void set_client_limit(size_t max_clients);
  void set_unlimited_clients();

  // Port retry configuration
  TcpServer& enable_port_retry(bool enable = true, int max_retries = 3, int retry_interval_ms = 1000);

  // Server state checking
  bool is_listening() const;

 private:
  void setup_internal_handlers();
  void handle_bytes(const uint8_t* data, size_t size);
  void handle_state(common::LinkState state);

  std::shared_ptr<interface::Channel> channel_;
  uint16_t port_;
  bool started_{false};
  bool auto_start_{false};
  bool auto_manage_{false};

  // Port retry configuration
  bool port_retry_enabled_{false};
  int max_port_retries_{3};
  int port_retry_interval_ms_{1000};

  // Client limit configuration
  bool client_limit_enabled_{false};
  size_t max_clients_{0};

  // Server state tracking
  bool is_listening_{false};

  // 사용자 콜백들
  DataHandler on_data_;
  ConnectHandler on_connect_;
  DisconnectHandler on_disconnect_;
  ErrorHandler on_error_;

  // 멀티 클라이언트 콜백들
  MultiClientConnectHandler on_multi_connect_;
  MultiClientDataHandler on_multi_data_;
  MultiClientDisconnectHandler on_multi_disconnect_;
};

}  // namespace wrapper
}  // namespace unilink
