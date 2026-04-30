#pragma once

/**
 * @file detail/mutex.hpp
 * @brief Selects the mutex type used for per-source file locking.
 *
 * Compile-time flag:
 *   AKASHA_THREAD_SAFE  — uses std::shared_mutex for full thread-safety.
 *                         Required for multithreaded use; has locking overhead.
 *   (default)           — uses NoOpSharedMutex (zero overhead). Safe only for
 *                         single-threaded access; unsafe for concurrent threads.
 *
 * Usage: -DAKASHA_THREAD_SAFE (compiler flag) or via CMake option
 *        AKASHA_THREAD_SAFE=ON.
 */

#ifdef AKASHA_THREAD_SAFE
#include <shared_mutex>
#endif

namespace akasha::detail {

#ifndef AKASHA_THREAD_SAFE

/**
 * @brief Drop-in no-op replacement for std::shared_mutex.
 *
 * Satisfies both BasicLockable and SharedMutex concepts so it can be used
 * with std::unique_lock<> and std::shared_lock<> without changes to call
 * sites. All operations are inlined no-ops — the compiler eliminates them
 * entirely in optimized builds.
 *
 * Default behavior: zero-overhead for single-threaded use.
 */
struct NoOpSharedMutex {
    constexpr void lock()          noexcept {}
    constexpr void unlock()        noexcept {}
    constexpr void lock_shared()   noexcept {}
    constexpr void unlock_shared() noexcept {}
    [[nodiscard]] constexpr bool try_lock()        noexcept { return true; }
    [[nodiscard]] constexpr bool try_lock_shared() noexcept { return true; }
};

using FileLockMutex = NoOpSharedMutex;

#else

using FileLockMutex = std::shared_mutex;

#endif

}  // namespace akasha::detail
