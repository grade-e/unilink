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

// #449: a blocking send (Reliable-mode send()/send_blocking()/send_move()/
// send_shared()) called from inside a data/message callback deadlocks -
// clearing backpressure requires progress on the same io thread that a
// blocking wait would now be stuck on. This thread_local flag, set for the
// duration of any data/message callback dispatch, lets the blocking-send
// path detect that scenario and fail fast (return false) instead of
// blocking forever. It intentionally isn't scoped per-channel: if the
// current thread is inside ANY callback dispatch, that thread can't make
// progress on I/O regardless of which channel triggered the callback -
// including a second channel sharing the same io_context/thread.
namespace wirestead {
namespace wrapper {
namespace detail {

// A depth counter rather than a bool so that a nested/reentrant
// CallbackGuard on the same thread doesn't clear the flag out from under an
// outer guard that's still in scope: the inner guard's destructor would
// otherwise flip g_callback_depth back to "not in a callback" while the
// outer dispatch is still running, reopening the #449 deadlock this guard
// exists to prevent.
inline thread_local int g_callback_depth = 0;

class CallbackGuard {
 public:
  CallbackGuard() { ++g_callback_depth; }
  ~CallbackGuard() { --g_callback_depth; }
  CallbackGuard(const CallbackGuard&) = delete;
  CallbackGuard& operator=(const CallbackGuard&) = delete;
};

inline bool in_data_callback() { return g_callback_depth > 0; }

}  // namespace detail
}  // namespace wrapper
}  // namespace wirestead
