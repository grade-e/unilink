#include "error_handler.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace unilink {
namespace common {

ErrorHandler& ErrorHandler::instance() {
  static ErrorHandler instance;
  return instance;
}

void ErrorHandler::report_error(const ErrorInfo& error) {
  if (!enabled_.load()) {
    return;
  }

  if (error.level < min_level_.load()) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    update_stats(error);
    add_to_recent_errors(error);
    add_to_component_errors(error);
    notify_callbacks(error);
  }
}

void ErrorHandler::register_callback(ErrorCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  callbacks_.push_back(std::move(callback));
}

void ErrorHandler::clear_callbacks() {
  std::lock_guard<std::mutex> lock(mutex_);
  callbacks_.clear();
}

void ErrorHandler::set_min_error_level(ErrorLevel level) { min_level_.store(level); }

ErrorLevel ErrorHandler::get_min_error_level() const { return min_level_.load(); }

void ErrorHandler::set_enabled(bool enabled) { enabled_.store(enabled); }

bool ErrorHandler::is_enabled() const { return enabled_.load(); }

ErrorStats ErrorHandler::get_error_stats() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  return stats_;
}

void ErrorHandler::reset_stats() {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  stats_.reset();
}

std::vector<ErrorInfo> ErrorHandler::get_errors_by_component(const std::string& component) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = errors_by_component_.find(component);
  if (it != errors_by_component_.end()) {
    return it->second;
  }
  return {};
}

std::vector<ErrorInfo> ErrorHandler::get_recent_errors(size_t count) const {
  std::lock_guard<std::mutex> lock(mutex_);

  size_t start_index = 0;
  if (recent_errors_.size() > count) {
    start_index = recent_errors_.size() - count;
  }

  return std::vector<ErrorInfo>(recent_errors_.begin() + start_index, recent_errors_.end());
}

bool ErrorHandler::has_errors(const std::string& component) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = errors_by_component_.find(component);
  return it != errors_by_component_.end() && !it->second.empty();
}

size_t ErrorHandler::get_error_count(const std::string& component, ErrorLevel level) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = errors_by_component_.find(component);
  if (it == errors_by_component_.end()) {
    return 0;
  }

  return std::count_if(it->second.begin(), it->second.end(),
                       [level](const ErrorInfo& error) { return error.level == level; });
}

void ErrorHandler::update_stats(const ErrorInfo& error) {
  std::lock_guard<std::mutex> lock(stats_mutex_);

  stats_.total_errors++;
  stats_.errors_by_level[static_cast<int>(error.level)]++;
  stats_.errors_by_category[static_cast<int>(error.category)]++;

  if (error.retryable) {
    stats_.retryable_errors++;
  }

  if (stats_.first_error == std::chrono::system_clock::time_point{}) {
    stats_.first_error = error.timestamp;
  }
  stats_.last_error = error.timestamp;
}

void ErrorHandler::notify_callbacks(const ErrorInfo& error) {
  for (const auto& callback : callbacks_) {
    try {
      callback(error);
    } catch (const std::exception& e) {
      // Avoid infinite recursion - log to stderr instead of using error handler
      std::cerr << "Error in error callback: " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "Unknown error in error callback" << std::endl;
    }
  }
}

void ErrorHandler::add_to_recent_errors(const ErrorInfo& error) {
  recent_errors_.push_back(error);

  // Keep only the most recent errors
  if (recent_errors_.size() > MAX_RECENT_ERRORS) {
    recent_errors_.erase(recent_errors_.begin(), recent_errors_.begin() + (recent_errors_.size() - MAX_RECENT_ERRORS));
  }
}

void ErrorHandler::add_to_component_errors(const ErrorInfo& error) {
  errors_by_component_[error.component].push_back(error);

  // Limit component error history to prevent memory growth
  constexpr size_t MAX_COMPONENT_ERRORS = 100;
  auto& component_errors = errors_by_component_[error.component];
  if (component_errors.size() > MAX_COMPONENT_ERRORS) {
    component_errors.erase(component_errors.begin(),
                           component_errors.begin() + (component_errors.size() - MAX_COMPONENT_ERRORS));
  }
}

// Convenience functions implementation
namespace error_reporting {

void report_connection_error(const std::string& component, const std::string& operation,
                             const boost::system::error_code& ec, bool retryable) {
  ErrorInfo error(ErrorLevel::ERROR, ErrorCategory::CONNECTION, component, operation, ec.message(), ec, retryable);
  ErrorHandler::instance().report_error(error);
}

void report_communication_error(const std::string& component, const std::string& operation, const std::string& message,
                                bool retryable) {
  ErrorInfo error(ErrorLevel::ERROR, ErrorCategory::COMMUNICATION, component, operation, message);
  error.retryable = retryable;
  ErrorHandler::instance().report_error(error);
}

void report_configuration_error(const std::string& component, const std::string& operation,
                                const std::string& message) {
  ErrorInfo error(ErrorLevel::ERROR, ErrorCategory::CONFIGURATION, component, operation, message);
  ErrorHandler::instance().report_error(error);
}

void report_memory_error(const std::string& component, const std::string& operation, const std::string& message) {
  ErrorInfo error(ErrorLevel::CRITICAL, ErrorCategory::MEMORY, component, operation, message);
  ErrorHandler::instance().report_error(error);
}

void report_system_error(const std::string& component, const std::string& operation, const std::string& message,
                         const boost::system::error_code& ec) {
  ErrorInfo error(ErrorLevel::ERROR, ErrorCategory::SYSTEM, component, operation, message, ec);
  ErrorHandler::instance().report_error(error);
}

void report_warning(const std::string& component, const std::string& operation, const std::string& message) {
  ErrorInfo error(ErrorLevel::WARNING, ErrorCategory::UNKNOWN, component, operation, message);
  ErrorHandler::instance().report_error(error);
}

void report_info(const std::string& component, const std::string& operation, const std::string& message) {
  ErrorInfo error(ErrorLevel::INFO, ErrorCategory::UNKNOWN, component, operation, message);
  ErrorHandler::instance().report_error(error);
}

}  // namespace error_reporting

}  // namespace common
}  // namespace unilink
