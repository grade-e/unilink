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

#include <string>

#include "wirestead/base/constants.hpp"
#include "wirestead/util/input_validator.hpp"

namespace wirestead {
namespace config {

struct SerialConfig {
#ifdef _WIN32
  std::string device = "COM1";
#else
  std::string device = "/dev/ttyUSB0";
#endif
  unsigned baud_rate = 115200;
  unsigned char_size = 8;  // 5,6,7,8
  enum class Parity { None, Even, Odd } parity = Parity::None;
  unsigned stop_bits = 1;  // 1 or 2
  enum class Flow { None, Software, Hardware } flow = Flow::None;

  size_t read_chunk = base::constants::DEFAULT_READ_BUFFER_SIZE;

  // Ask the kernel driver to hand bytes up as soon as they arrive instead of
  // waiting out its buffering timer. On Linux this is ASYNC_LOW_LATENCY, and
  // it matters most on USB serial adapters: an FTDI defaults to a 16 ms
  // latency timer, so a 1 ms packet at 115200 baud can still reach the
  // callback 16 ms late — enough to break a control loop on its own.
  //
  // Best effort by design. Drivers that have no such timer (CDC-ACM, most
  // native UARTs) reject the request, and that is not an error: the port opens
  // and runs normally either way. Set false to leave the driver's default
  // alone, which trades latency for fewer wakeups.
  bool low_latency = true;

  bool reopen_on_error = true;  // Attempt to reopen on device disconnection/error
  size_t backpressure_threshold = base::constants::DEFAULT_BACKPRESSURE_THRESHOLD;
  base::constants::BackpressureStrategy backpressure_strategy = base::constants::BackpressureStrategy::Reliable;
  bool enable_memory_pool = true;
  // Controls whether callback exceptions halt the link (true) or trigger the normal retry flow (false)
  bool stop_on_callback_exception = false;

  unsigned retry_interval_ms = base::constants::DEFAULT_RETRY_INTERVAL_MS;
  int max_retries = base::constants::DEFAULT_MAX_RETRIES;

  // Opt into the shared IoContextManager singleton instead of a dedicated
  // io_context + thread (the default since #440). Only meaningful for
  // deliberately trading per-instance parallelism for reduced thread/memory
  // overhead across many instances in one process.
  bool use_shared_context = false;

  // Validation methods
  bool is_valid() const {
    return read_chunk >= base::constants::MIN_READ_BUFFER_SIZE && read_chunk <= base::constants::MAX_READ_BUFFER_SIZE &&
           util::InputValidator::is_valid_device_path(device) && baud_rate >= base::constants::MIN_BAUD_RATE &&
           baud_rate <= base::constants::MAX_BAUD_RATE && char_size >= 5 && char_size <= 8 &&
           (stop_bits == 1 || stop_bits == 2) && retry_interval_ms >= base::constants::MIN_RETRY_INTERVAL_MS &&
           retry_interval_ms <= base::constants::MAX_RETRY_INTERVAL_MS &&
           backpressure_threshold >= base::constants::MIN_BACKPRESSURE_THRESHOLD &&
           backpressure_threshold <= base::constants::MAX_BACKPRESSURE_THRESHOLD &&
           (max_retries == -1 || (max_retries >= 0 && max_retries <= base::constants::MAX_RETRIES_LIMIT));
  }

  // Apply validation and clamp values to valid ranges
  void validate_and_clamp() {
    // Same bounds the TCP and UDS configs apply to read_buffer_size: this is
    // the per-connection userspace read buffer under a different name, and an
    // unclamped 0 reaches the transport as rx_.resize(0).
    if (read_chunk < base::constants::MIN_READ_BUFFER_SIZE) {
      read_chunk = base::constants::MIN_READ_BUFFER_SIZE;
    } else if (read_chunk > base::constants::MAX_READ_BUFFER_SIZE) {
      read_chunk = base::constants::MAX_READ_BUFFER_SIZE;
    }

    if (baud_rate < base::constants::MIN_BAUD_RATE) {
      baud_rate = base::constants::MIN_BAUD_RATE;
    } else if (baud_rate > base::constants::MAX_BAUD_RATE) {
      baud_rate = base::constants::MAX_BAUD_RATE;
    }

    if (char_size < 5)
      char_size = 5;
    else if (char_size > 8)
      char_size = 8;

    if (stop_bits != 1 && stop_bits != 2) stop_bits = 1;

    if (retry_interval_ms < base::constants::MIN_RETRY_INTERVAL_MS) {
      retry_interval_ms = base::constants::MIN_RETRY_INTERVAL_MS;
    } else if (retry_interval_ms > base::constants::MAX_RETRY_INTERVAL_MS) {
      retry_interval_ms = base::constants::MAX_RETRY_INTERVAL_MS;
    }

    if (backpressure_threshold < base::constants::MIN_BACKPRESSURE_THRESHOLD) {
      backpressure_threshold = base::constants::MIN_BACKPRESSURE_THRESHOLD;
    } else if (backpressure_threshold > base::constants::MAX_BACKPRESSURE_THRESHOLD) {
      backpressure_threshold = base::constants::MAX_BACKPRESSURE_THRESHOLD;
    }

    if (max_retries != -1 && max_retries > base::constants::MAX_RETRIES_LIMIT) {
      max_retries = base::constants::MAX_RETRIES_LIMIT;
    }
  }
};

}  // namespace config
}  // namespace wirestead
