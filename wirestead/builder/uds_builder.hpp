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
#include "wirestead/wrapper/uds_client/uds_client.hpp"
#include "wirestead/wrapper/uds_server/uds_server.hpp"

namespace wirestead {
namespace builder {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Modernized Builder for UdsClient
 */
class WIRESTEAD_API UdsClientBuilder : public BuilderInterface<wrapper::UdsClient, UdsClientBuilder> {
 public:
  explicit UdsClientBuilder(const std::string& socket_path);

  // Delete copy
  UdsClientBuilder(const UdsClientBuilder&) = delete;
  UdsClientBuilder& operator=(const UdsClientBuilder&) = delete;

  std::unique_ptr<wrapper::UdsClient> build() override;

  UdsClientBuilder& auto_start(bool auto_start = true) override;
  UdsClientBuilder& retry_interval(std::chrono::milliseconds interval);
  UdsClientBuilder& max_retries(int max_retries);
  UdsClientBuilder& connection_timeout(std::chrono::milliseconds timeout);

  /**
   * @brief Size of the userspace buffer each read fills, in bytes.
   *
   * Raising this reduces read completions and callback dispatches on bulk
   * transfers, at the cost of that much memory per connection. Clamped to
   * [MIN_READ_BUFFER_SIZE, MAX_READ_BUFFER_SIZE].
   */
  UdsClientBuilder& read_buffer_size(size_t bytes);
  UdsClientBuilder& independent_context(bool use_independent = true);

 private:
  std::string socket_path_;
  bool auto_start_;
  bool independent_context_;

  std::chrono::milliseconds retry_interval_;
  bool retry_interval_set_{false};
  int max_retries_;
  bool max_retries_set_{false};
  std::chrono::milliseconds connection_timeout_;
  bool connection_timeout_set_{false};
  size_t read_buffer_size_;
  bool read_buffer_size_set_{false};
};

using UdsClientBuilderDefault = UdsClientBuilder;

/**
 * @brief Modernized Builder for UdsServer
 */
class WIRESTEAD_API UdsServerBuilder : public BuilderInterface<wrapper::UdsServer, UdsServerBuilder> {
 public:
  explicit UdsServerBuilder(const std::string& socket_path);

  // Delete copy
  UdsServerBuilder(const UdsServerBuilder&) = delete;
  UdsServerBuilder& operator=(const UdsServerBuilder&) = delete;

  std::unique_ptr<wrapper::UdsServer> build() override;

  UdsServerBuilder& auto_start(bool auto_start = true) override;
  UdsServerBuilder& independent_context(bool use_independent = true);
  UdsServerBuilder& idle_timeout(std::chrono::milliseconds timeout);

  /**
   * @brief Size of the userspace buffer each read fills, in bytes.
   *
   * Raising this reduces read completions and callback dispatches on bulk
   * transfers, at the cost of that much memory per connection. Clamped to
   * [MIN_READ_BUFFER_SIZE, MAX_READ_BUFFER_SIZE].
   */
  UdsServerBuilder& read_buffer_size(size_t bytes);
  UdsServerBuilder& max_clients(uint32_t max_clients);
  [[deprecated("Use max_clients(1) instead")]]
  UdsServerBuilder& single_client();
  [[deprecated("Use max_clients(max) instead")]]
  UdsServerBuilder& multi_client(size_t max);

 private:
  std::string socket_path_;
  bool auto_start_;
  bool independent_context_;

  uint32_t max_clients_;
  bool client_limit_enabled_;
  std::chrono::milliseconds idle_timeout_;
  bool idle_timeout_set_;
  size_t read_buffer_size_;
  bool read_buffer_size_set_{false};
};

using UdsServerBuilderDefault = UdsServerBuilder;

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace builder
}  // namespace wirestead
