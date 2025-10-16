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

#include "unilink/common/input_validator.hpp"

#include <regex>
#include <sstream>
#include <string>

namespace unilink {
namespace common {

void InputValidator::validate_host(const std::string& host) {
  validate_non_empty_string(host, "host");
  validate_string_length(host, MAX_HOSTNAME_LENGTH, "host");

  // Check if it's an IPv4 address
  if (is_valid_ipv4(host)) {
    return;
  }

  // Check if it's an IPv6 address
  if (is_valid_ipv6(host)) {
    return;
  }

  // Check if it's a valid hostname
  if (is_valid_hostname(host)) {
    return;
  }

  throw ValidationException("invalid host format", "host", "valid IPv4, IPv6, or hostname");
}

void InputValidator::validate_ipv4_address(const std::string& address) {
  validate_non_empty_string(address, "ipv4_address");

  if (!is_valid_ipv4(address)) {
    throw ValidationException("invalid IPv4 address format", "ipv4_address", "valid IPv4 address");
  }
}

void InputValidator::validate_ipv6_address(const std::string& address) {
  validate_non_empty_string(address, "ipv6_address");

  if (!is_valid_ipv6(address)) {
    throw ValidationException("invalid IPv6 address format", "ipv6_address", "valid IPv6 address");
  }
}

void InputValidator::validate_device_path(const std::string& device) {
  validate_non_empty_string(device, "device_path");
  validate_string_length(device, MAX_DEVICE_PATH_LENGTH, "device_path");

  if (!is_valid_device_path(device)) {
    throw ValidationException("invalid device path format", "device_path", "valid device path");
  }
}

void InputValidator::validate_parity(const std::string& parity) {
  validate_non_empty_string(parity, "parity");

  // Convert to lowercase for case-insensitive comparison
  std::string lower_parity = parity;
  std::transform(lower_parity.begin(), lower_parity.end(), lower_parity.begin(), ::tolower);

  if (lower_parity != "none" && lower_parity != "odd" && lower_parity != "even") {
    throw ValidationException("invalid parity value", "parity", "none, odd, or even");
  }
}

bool InputValidator::is_valid_ipv4(const std::string& address) {
  // Split the address into octets
  std::stringstream ss(address);
  std::string octet;
  std::vector<std::string> octets;

  while (std::getline(ss, octet, '.')) {
    octets.push_back(octet);
  }

  // Must have exactly 4 octets
  if (octets.size() != 4) {
    return false;
  }

  // Validate each octet
  for (const auto& oct : octets) {
    // Check for empty octet
    if (oct.empty()) {
      return false;
    }

    // Check for leading zeros (except for "0" itself)
    if (oct.length() > 1 && oct[0] == '0') {
      return false;
    }

    // Check for non-numeric characters
    for (char c : oct) {
      if (!std::isdigit(c)) {
        return false;
      }
    }

    // Check for valid range
    try {
      // Use long long to handle potential overflow
      long long value = std::stoll(oct);
      if (value < 0 || value > 255) {
        return false;
      }
    } catch (const std::exception&) {
      return false;
    }
  }

  return true;
}

bool InputValidator::is_valid_ipv6(const std::string& address) {
  // Simplified IPv6 validation - this is a basic check
  // Full IPv6 validation is complex and would require more sophisticated parsing
  std::regex ipv6_pattern(R"(^([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$|^::1$|^::$)");
  return std::regex_match(address, ipv6_pattern);
}

bool InputValidator::is_valid_hostname(const std::string& hostname) {
  // Hostname validation according to RFC 1123
  // - Must not be empty
  // - Must not start or end with hyphen
  // - Must contain only alphanumeric characters and hyphens
  // - Each label must be 1-63 characters
  // - Total length must not exceed 253 characters

  if (hostname.empty() || hostname.length() > MAX_HOSTNAME_LENGTH) {
    return false;
  }

  if (hostname.front() == '-' || hostname.back() == '-') {
    return false;
  }

  // Check each label (separated by dots)
  std::stringstream ss(hostname);
  std::string label;

  while (std::getline(ss, label, '.')) {
    if (label.empty() || label.length() > 63) {
      return false;
    }

    // Check if label contains only valid characters
    for (char c : label) {
      if (!std::isalnum(c) && c != '-') {
        return false;
      }
    }
  }

  return true;
}

bool InputValidator::is_valid_device_path(const std::string& device) {
  // Basic device path validation
  // - Must not be empty
  // - Must start with '/' (Unix-style) or be a COM port (Windows-style)
  // - Must not contain invalid characters

  if (device.empty()) {
    return false;
  }

  // Unix-style device path (e.g., /dev/ttyUSB0, /dev/ttyACM0)
  if (device.front() == '/') {
    // Check for valid Unix device path characters
    for (char c : device) {
      if (!std::isalnum(c) && c != '/' && c != '_' && c != '-') {
        return false;
      }
    }
    return true;
  }

  // Windows-style COM port (e.g., COM1, COM2, etc.)
  if (device.length() >= 4 && device.substr(0, 3) == "COM") {
    std::string port_num = device.substr(3);
    try {
      int port = std::stoi(port_num);
      return port >= 1 && port <= 255;
    } catch (const std::exception&) {
      return false;
    }
  }

  // Windows special device names
  if (device == "NUL" || device == "CON" || device == "PRN" || device == "AUX" || device == "LPT1" ||
      device == "LPT2" || device == "LPT3") {
    return true;
  }

  return false;
}

}  // namespace common
}  // namespace unilink
