#pragma once

#include <cstdint>
#include <regex>
#include <string>

#include "unilink/common/constants.hpp"
#include "unilink/common/exceptions.hpp"

namespace unilink {
namespace common {

/**
 * @brief Input validation utility class
 *
 * Provides comprehensive input validation for all unilink components.
 * Throws ValidationException for invalid inputs with detailed error messages.
 */
class InputValidator {
 public:
  // Network validation
  static void validate_host(const std::string& host);
  static void validate_port(uint16_t port);
  static void validate_ipv4_address(const std::string& address);
  static void validate_ipv6_address(const std::string& address);

  // Serial validation
  static void validate_device_path(const std::string& device);
  static void validate_baud_rate(uint32_t baud_rate);
  static void validate_data_bits(uint8_t data_bits);
  static void validate_stop_bits(uint8_t stop_bits);
  static void validate_parity(const std::string& parity);

  // Memory validation
  static void validate_buffer_size(size_t size);
  static void validate_memory_alignment(const void* ptr, size_t alignment);

  // Timeout and interval validation
  static void validate_timeout(unsigned timeout_ms);
  static void validate_retry_interval(unsigned interval_ms);
  static void validate_retry_count(int retry_count);

  // String validation
  static void validate_non_empty_string(const std::string& str, const std::string& field_name);
  static void validate_string_length(const std::string& str, size_t max_length, const std::string& field_name);

  // Numeric validation
  static void validate_positive_number(int64_t value, const std::string& field_name);
  static void validate_range(int64_t value, int64_t min, int64_t max, const std::string& field_name);
  static void validate_range(size_t value, size_t min, size_t max, const std::string& field_name);

 private:
  // Helper methods
  static bool is_valid_ipv4(const std::string& address);
  static bool is_valid_ipv6(const std::string& address);
  static bool is_valid_hostname(const std::string& hostname);
  static bool is_valid_device_path(const std::string& device);

  // Constants
  static constexpr size_t MAX_HOSTNAME_LENGTH = 253;
  static constexpr size_t MAX_DEVICE_PATH_LENGTH = 256;
  static constexpr size_t MAX_BUFFER_SIZE = 64 * 1024 * 1024;  // 64MB
  static constexpr size_t MIN_BUFFER_SIZE = 1;
  static constexpr uint32_t MIN_BAUD_RATE = 50;
  static constexpr uint32_t MAX_BAUD_RATE = 4000000;
  static constexpr uint8_t MIN_DATA_BITS = 5;
  static constexpr uint8_t MAX_DATA_BITS = 8;
  static constexpr uint8_t MIN_STOP_BITS = 1;
  static constexpr uint8_t MAX_STOP_BITS = 2;
  static constexpr unsigned MAX_TIMEOUT_MS = 300000;  // 5 minutes
  static constexpr unsigned MIN_TIMEOUT_MS = 1;
  static constexpr unsigned MAX_RETRY_INTERVAL_MS = 300000;  // 5 minutes
  static constexpr unsigned MIN_RETRY_INTERVAL_MS = 1;
  static constexpr int MAX_RETRY_COUNT = 10000;
  static constexpr int MIN_RETRY_COUNT = -1;  // -1 means unlimited
};

// Inline implementations for simple validations
inline void InputValidator::validate_non_empty_string(const std::string& str, const std::string& field_name) {
  if (str.empty()) {
    throw ValidationException(field_name + " cannot be empty", field_name, "non-empty string");
  }
}

inline void InputValidator::validate_string_length(const std::string& str, size_t max_length,
                                                   const std::string& field_name) {
  if (str.length() > max_length) {
    throw ValidationException(field_name + " length exceeds maximum allowed length", field_name,
                              "length <= " + std::to_string(max_length));
  }
}

inline void InputValidator::validate_positive_number(int64_t value, const std::string& field_name) {
  if (value <= 0) {
    throw ValidationException(field_name + " must be positive", field_name, "positive number");
  }
}

inline void InputValidator::validate_range(int64_t value, int64_t min, int64_t max, const std::string& field_name) {
  if (value < min || value > max) {
    throw ValidationException(field_name + " out of range", field_name,
                              std::to_string(min) + " <= value <= " + std::to_string(max));
  }
}

inline void InputValidator::validate_range(size_t value, size_t min, size_t max, const std::string& field_name) {
  if (value < min || value > max) {
    throw ValidationException(field_name + " out of range", field_name,
                              std::to_string(min) + " <= value <= " + std::to_string(max));
  }
}

inline void InputValidator::validate_buffer_size(size_t size) {
  validate_range(size, constants::MIN_BUFFER_SIZE, constants::MAX_BUFFER_SIZE, "buffer_size");
}

inline void InputValidator::validate_timeout(unsigned timeout_ms) {
  validate_range(static_cast<int64_t>(timeout_ms), constants::MIN_CONNECTION_TIMEOUT_MS,
                 constants::MAX_CONNECTION_TIMEOUT_MS, "timeout_ms");
}

inline void InputValidator::validate_retry_interval(unsigned interval_ms) {
  validate_range(static_cast<int64_t>(interval_ms), constants::MIN_RETRY_INTERVAL_MS, constants::MAX_RETRY_INTERVAL_MS,
                 "retry_interval_ms");
}

inline void InputValidator::validate_retry_count(int retry_count) {
  if (retry_count < 0 || retry_count > 100) {
    throw ValidationException("retry_count out of range", "retry_count", "0 <= retry_count <= 100");
  }
}

inline void InputValidator::validate_port(uint16_t port) {
  if (port == 0) {
    throw ValidationException("port cannot be zero", "port", "non-zero port number");
  }
  // Port numbers are already constrained by uint16_t type (0-65535)
}

inline void InputValidator::validate_baud_rate(uint32_t baud_rate) {
  validate_range(static_cast<int64_t>(baud_rate), MIN_BAUD_RATE, MAX_BAUD_RATE, "baud_rate");
}

inline void InputValidator::validate_data_bits(uint8_t data_bits) {
  validate_range(static_cast<int64_t>(data_bits), MIN_DATA_BITS, MAX_DATA_BITS, "data_bits");
}

inline void InputValidator::validate_stop_bits(uint8_t stop_bits) {
  validate_range(static_cast<int64_t>(stop_bits), MIN_STOP_BITS, MAX_STOP_BITS, "stop_bits");
}

inline void InputValidator::validate_memory_alignment(const void* ptr, size_t alignment) {
  if (ptr == nullptr) {
    throw ValidationException("memory pointer cannot be null", "ptr", "non-null pointer");
  }

  uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
  if (address % alignment != 0) {
    throw ValidationException("memory pointer not properly aligned", "ptr",
                              "aligned to " + std::to_string(alignment) + " bytes");
  }
}

}  // namespace common
}  // namespace unilink
