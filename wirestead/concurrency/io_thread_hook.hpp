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

#include "wirestead/base/visibility.hpp"

namespace wirestead {
namespace concurrency {

/**
 * @brief Run this on every io thread the library starts, before it runs any
 *        work.
 *
 * The library creates its own threads, so nothing else can reach them: a
 * deployment that needs `SCHED_FIFO`, a CPU affinity mask, or just a readable
 * name in `top` had no handle to apply it to. The hook runs on the new thread
 * itself, which is what those APIs want - `pthread_setschedparam(pthread_self(),
 * ...)`, `sched_setaffinity(0, ...)`, `pthread_setname_np(pthread_self(), ...)`.
 *
 * ```cpp
 * wirestead::concurrency::set_io_thread_init([] {
 *   pthread_setname_np(pthread_self(), "wirestead-io");
 *   sched_param p{.sched_priority = 20};
 *   pthread_setschedparam(pthread_self(), SCHED_FIFO, &p);
 * });
 * ```
 *
 * Deliberately process-wide rather than per channel. Thread policy is a
 * property of the deployment, not of one connection, and a per-config field
 * would have to be threaded through six transports and their builders to say
 * the same thing.
 *
 * Set it before starting any channel. Installing a hook does not reach threads
 * that are already running, and the library never removes what the hook did -
 * a raised priority outlives the channel that triggered it if the thread is a
 * shared one.
 *
 * The hook runs on a thread that is about to service io. Blocking in it blocks
 * that channel's io, and an exception escaping it would terminate the process,
 * so exceptions are caught and swallowed. Pass `nullptr` to clear.
 *
 * Thread-safe: may be called while other threads are starting, though the
 * ordering above is what makes it useful.
 */
WIRESTEAD_API void set_io_thread_init(std::function<void()> fn);

/**
 * @brief Invoke the installed hook, if any. Called by the library on each io
 *        thread it starts; not intended for user code.
 */
WIRESTEAD_API void run_io_thread_init() noexcept;

}  // namespace concurrency
}  // namespace wirestead
