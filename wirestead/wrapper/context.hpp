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
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "wirestead/base/common.hpp"
#include "wirestead/base/error_codes.hpp"
#include "wirestead/memory/safe_data_buffer.hpp"
#include "wirestead/memory/safe_span.hpp"

namespace wirestead {
namespace wrapper {

/**
 * @brief Context for data/message related events
 */
class MessageContext {
 public:
  /**
   * @brief Construct a context that owns a copy of the payload.
   *
   * Use this whenever the context outlives the callback that produced the
   * data — the batch queues hold contexts until the batch is flushed, so they
   * must own.
   */
  MessageContext(ClientId client_id, memory::SafeDataBuffer data, std::string client_info = "")
      : client_id_(client_id), owned_(std::move(data)), client_info_(std::move(client_info)) {}

  /**
   * @brief Construct a context that borrows the payload without copying it.
   *
   * `data` must stay valid for the whole lifetime of this MessageContext. That
   * holds for single-shot on_data()/on_message() dispatch, where the context is
   * a temporary destroyed before the transport reuses its receive buffer, so
   * those paths cost no per-chunk allocation at all. Do not use this
   * constructor for a context that is queued or otherwise outlives the
   * callback — use the SafeDataBuffer constructor above for those.
   */
  MessageContext(ClientId client_id, memory::ConstByteSpan data, std::string client_info = "")
      : client_id_(client_id), view_(data), client_info_(std::move(client_info)) {}

  // A copy always owns its payload, even when the source only borrows. The
  // callback receives a `const MessageContext&`, so copying is the only way a
  // caller can keep a context past the callback, and that is exactly when the
  // borrowed bytes stop being valid. Doing this in the copy costs the hot path
  // nothing: dispatch passes the context by reference and never copies it.
  MessageContext(const MessageContext& other)
      : client_id_(other.client_id_),
        owned_(other.cloned_payload()),
        client_info_(other.client_info_),
        received_at_(other.received_at_) {}

  MessageContext& operator=(const MessageContext& other) {
    if (this != &other) {
      client_id_ = other.client_id_;
      owned_ = other.cloned_payload();
      view_ = {};
      client_info_ = other.client_info_;
      received_at_ = other.received_at_;
    }
    return *this;
  }

  // Moves stay cheap and noexcept so the batch queues keep using them on
  // vector reallocation instead of falling back to the copy above.
  MessageContext(MessageContext&&) noexcept = default;
  MessageContext& operator=(MessageContext&&) noexcept = default;
  ~MessageContext() = default;

  /** @brief Get the client identifier */
  ClientId client_id() const { return client_id_; }

  /**
   * @brief Get the received data as a view.
   *
   * The returned view is only valid for the lifetime of this MessageContext.
   * Do not store it past the callback's return — use data_as_string() instead.
   */
  std::string_view data() const {
    const auto size = payload_size();
    if (size == 0) return {};
    return std::string_view(reinterpret_cast<const char*>(payload_data()), size);
  }

  /**
   * @brief Get the received data as a SafeDataBuffer reference.
   *
   * When this context borrows its payload (the single-shot callback paths),
   * the buffer is materialized on first use and cached, so this accessor is
   * the one place that still pays for a copy. Prefer data(), data_as_string(),
   * or data_as_vector(). Materialization is not synchronized; like every other
   * accessor here it assumes the single callback thread the context is
   * delivered on (see docs/callbacks.md).
   */
  const memory::SafeDataBuffer& safe_data() const {
    if (!owned_) owned_.emplace(view_);
    return *owned_;
  }

  /** @brief Get the received data as a std::string */
  std::string data_as_string() const {
    const auto size = payload_size();
    if (size == 0) return {};
    return std::string(reinterpret_cast<const char*>(payload_data()), size);
  }

  /** @brief Get the received data as a std::vector<uint8_t> */
  std::vector<uint8_t> data_as_vector() const {
    const auto* d = payload_data();
    return std::vector<uint8_t>(d, d + payload_size());
  }

  /** @brief Get the client information (e.g., endpoint address) */
  const std::string& client_info() const { return client_info_; }

  /**
   * @brief When this payload arrived, on the steady clock.
   *
   * Stamped where the context is constructed, which on every receive path is
   * the moment the bytes came off the transport — not the moment the callback
   * runs. The difference is what makes it worth having:
   *
   * - a batch callback delivers contexts stamped when each chunk arrived, not
   *   when the batch flushed, so a whole batch does not collapse onto one time;
   * - a framer that finds several messages in one read gives each of them that
   *   read's arrival time, where a clock read inside the callback would be
   *   timing the parse instead.
   *
   * Steady, not system: stamping is exactly where a clock step would otherwise
   * corrupt the data, and a difference between two steady readings survives
   * one. To place it on a wall clock — a ROS header stamp, say — subtract the
   * age rather than converting the epoch:
   *
   * ```cpp
   * const auto age = std::chrono::steady_clock::now() - ctx.received_at();
   * msg.header.stamp = node->now() - rclcpp::Duration(age);
   * ```
   */
  std::chrono::steady_clock::time_point received_at() const { return received_at_; }

 private:
  const uint8_t* payload_data() const noexcept { return owned_ ? owned_->data() : view_.data(); }
  size_t payload_size() const noexcept { return owned_ ? owned_->size() : view_.size(); }

  // Payload for a copy: reuse the owned buffer when there is one, otherwise
  // take a copy of the borrowed bytes.
  std::optional<memory::SafeDataBuffer> cloned_payload() const {
    if (owned_) return owned_;
    return memory::SafeDataBuffer(view_);
  }

  ClientId client_id_;
  // Exactly one of these carries the payload: `owned_` when engaged, `view_`
  // otherwise. `view_` is left default-constructed (and unread) on the owning
  // path, which keeps the default copy/move operations correct — they never
  // have to re-seat a pointer into relocated storage. `owned_` is mutable only
  // so safe_data() can materialize a borrowed payload on demand.
  mutable std::optional<memory::SafeDataBuffer> owned_;
  memory::ConstByteSpan view_;
  std::string client_info_;
  // Default-initialized rather than passed in, so every construction site
  // stamps arrival without threading a parameter through seven wrappers. The
  // copy operations below must carry it across explicitly - a copy that let
  // this initializer run again would silently re-stamp the payload with the
  // time it was copied, which is precisely the error this field exists to
  // prevent.
  std::chrono::steady_clock::time_point received_at_{std::chrono::steady_clock::now()};
};

/**
 * @brief Context for connection/disconnection events
 */
class ConnectionContext {
 public:
  ConnectionContext(ClientId client_id, std::string client_info = "")
      : client_id_(client_id), client_info_(std::move(client_info)) {}

  /** @brief Get the client identifier */
  ClientId client_id() const { return client_id_; }

  /** @brief Get the client information (e.g., endpoint address) */
  const std::string& client_info() const { return client_info_; }

 private:
  ClientId client_id_;
  std::string client_info_;
};

/**
 * @brief Context for error events
 */
class ErrorContext {
 public:
  ErrorContext(ErrorCode code, std::string_view message, std::optional<ClientId> client_id = std::nullopt)
      : code_(code), message_(message), client_id_(client_id) {}

  /** @brief Get the error code */
  ErrorCode code() const { return code_; }

  /** @brief Get the error message */
  std::string_view message() const { return message_; }

  /** @brief Get the associated client ID, if any */
  std::optional<ClientId> client_id() const { return client_id_; }

 private:
  ErrorCode code_;
  std::string message_;
  std::optional<ClientId> client_id_;
};

}  // namespace wrapper
}  // namespace wirestead
