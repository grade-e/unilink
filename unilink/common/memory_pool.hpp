#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace unilink {
namespace common {

/**
 * @brief Selective simplified memory pool with optimized performance
 *
 * Core design principles:
 * - Small pools: Lock-based (fast allocation, low overhead)
 * - Large pools: Lock-free (high concurrency)
 * - Memory alignment: 64-byte alignment for buffers >= 4KB
 * - Minimal statistics: Basic stats only to minimize overhead
 */
class MemoryPool {
 public:
  struct PoolStats {
    // Simplified core statistics only
    size_t total_allocations{0};
    size_t pool_hits{0};
    size_t pool_misses{0};
    size_t current_pool_size{0};
    size_t max_pool_size{0};
  };

  // Simplified health monitoring
  struct HealthMetrics {
    double pool_utilization{0.0};
    double hit_rate{0.0};
    double memory_efficiency{0.0};
    double performance_score{0.0};
  };

  // Simplified buffer info structure
  struct BufferInfo {
    std::unique_ptr<uint8_t[]> data;
    size_t size;
    std::chrono::steady_clock::time_point last_used;
    bool in_use{false};
    BufferInfo* next_free{nullptr};

    BufferInfo() = default;
    BufferInfo(BufferInfo&& other) noexcept;
    BufferInfo& operator=(BufferInfo&& other) noexcept;
    BufferInfo(const BufferInfo&) = delete;
    BufferInfo& operator=(const BufferInfo&) = delete;
  };

  // Predefined buffer sizes for common use cases
  enum class BufferSize : size_t {
    SMALL = 1024,   // 1KB - small messages
    MEDIUM = 4096,  // 4KB - typical network packets
    LARGE = 16384,  // 16KB - large data transfers
    XLARGE = 65536  // 64KB - bulk operations
  };

  explicit MemoryPool(size_t initial_pool_size = 400, size_t max_pool_size = 2000);
  ~MemoryPool() = default;

  // Non-copyable, non-movable
  MemoryPool(const MemoryPool&) = delete;
  MemoryPool& operator=(const MemoryPool&) = delete;
  MemoryPool(MemoryPool&&) = delete;
  MemoryPool& operator=(MemoryPool&&) = delete;

  std::unique_ptr<uint8_t[]> acquire(size_t size);
  std::unique_ptr<uint8_t[]> acquire(BufferSize buffer_size);
  void release(std::unique_ptr<uint8_t[]> buffer, size_t size);
  PoolStats get_stats() const;
  double get_hit_rate() const;
  void cleanup_old_buffers(std::chrono::milliseconds max_age = std::chrono::minutes(5));
  std::pair<size_t, size_t> get_memory_usage() const;
  void resize_pool(size_t new_size);
  void auto_tune();
  HealthMetrics get_health_metrics() const;

 private:
  // Simplified pool bucket structure
  struct alignas(64) PoolBucket {
    std::vector<BufferInfo> buffers_;
    std::queue<size_t> free_indices_;
    mutable std::mutex mutex_;
    size_t size_;

    // Lock-free support for large pools
    std::vector<std::unique_ptr<uint8_t[]>> lock_free_pool_;
    std::atomic<size_t> lock_free_index_{0};
    std::atomic<size_t> lock_free_allocated_{0};
    bool use_lock_free_{false};

    // Usage time tracking for cleanup
    std::vector<std::chrono::steady_clock::time_point> last_used_times_;

    PoolBucket() = default;
    PoolBucket(PoolBucket&& other) noexcept;
    PoolBucket& operator=(PoolBucket&& other) noexcept;
    PoolBucket(const PoolBucket&) = delete;
    PoolBucket& operator=(const PoolBucket&) = delete;
  };

  std::array<PoolBucket, 4> buckets_;  // For SMALL, MEDIUM, LARGE, XLARGE
  size_t max_pool_size_;               // Total max buffers across all buckets
  PoolStats stats_;                    // Centralized simplified statistics

  // Constants
  static constexpr size_t ALIGNMENT_SIZE = 64;
  static constexpr size_t ALIGNMENT_THRESHOLD = 4096;         // Only align buffers >= 4KB
  static constexpr size_t LOCK_FREE_THRESHOLD = 1000;         // Use lock-free for pools >= 1000 buffers
  static constexpr size_t LOCK_FREE_POOL_INITIAL_SIZE = 100;  // Initial size for lock-free pool

  // Helper functions
  PoolBucket& get_bucket(size_t size);
  size_t get_bucket_index(size_t size) const;

  // Allocation functions
  std::unique_ptr<uint8_t[]> acquire_with_lock(PoolBucket& bucket);
  std::unique_ptr<uint8_t[]> acquire_lock_free(PoolBucket& bucket);
  std::unique_ptr<uint8_t[]> create_buffer(size_t size);
  std::unique_ptr<uint8_t[]> create_aligned_buffer(size_t size);

  // Release functions
  void release_with_lock(PoolBucket& bucket, std::unique_ptr<uint8_t[]> buffer);
  void release_lock_free(PoolBucket& bucket, std::unique_ptr<uint8_t[]> buffer);

  // Utility functions
  bool should_use_lock_free(size_t pool_size) const;
  bool should_use_aligned_allocation(size_t size) const;
  void validate_size(size_t size) const;
};

/**
 * @brief Global memory pool instance
 */
class GlobalMemoryPool {
 public:
  static MemoryPool& instance() {
    static MemoryPool pool;
    return pool;
  }

  // Factory method to create optimized memory pool
  static std::unique_ptr<MemoryPool> create_optimized() {
    return std::make_unique<MemoryPool>(800, 4000);  // Optimized default sizes
  }

  // Factory method to create size-optimized memory pool
  static std::unique_ptr<MemoryPool> create_size_optimized() {
    return std::make_unique<MemoryPool>(1200, 6000);  // Even larger for better concurrency
  }

  // Non-copyable, non-movable
  GlobalMemoryPool() = delete;
  GlobalMemoryPool(const GlobalMemoryPool&) = delete;
  GlobalMemoryPool& operator=(const GlobalMemoryPool&) = delete;
};

/**
 * @brief RAII wrapper for memory pool buffers with enhanced safety
 */
class PooledBuffer {
 public:
  explicit PooledBuffer(size_t size);
  explicit PooledBuffer(MemoryPool::BufferSize buffer_size);
  ~PooledBuffer();

  // Non-copyable, movable
  PooledBuffer(const PooledBuffer&) = delete;
  PooledBuffer& operator=(const PooledBuffer&) = delete;
  PooledBuffer(PooledBuffer&& other) noexcept;
  PooledBuffer& operator=(PooledBuffer&& other) noexcept;

  // Safe access methods
  uint8_t* data() const;
  size_t size() const;
  bool valid() const;

  // Safe array access with bounds checking
  uint8_t& operator[](size_t index);
  const uint8_t& operator[](size_t index) const;

  // Safe pointer arithmetic
  uint8_t* at(size_t offset) const;

  // Explicit conversion methods (no implicit conversion)
  uint8_t* get() const { return data(); }
  explicit operator bool() const { return valid(); }

 private:
  std::unique_ptr<uint8_t[]> buffer_;
  size_t size_;
  MemoryPool* pool_;

  // Helper for bounds checking
  void check_bounds(size_t index) const;
};

}  // namespace common
}  // namespace unilink
