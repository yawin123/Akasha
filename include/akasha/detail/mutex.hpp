#pragma once

/**
 * @file detail/mutex.hpp
 * @brief Selects the mutex type used for per-source file locking.
 *
 * Compile-time flag:
 *   AKASHA_SINGLE_THREAD  — replaces all file/source mutexes with a no-op
 *                           implementation. Zero overhead; unsafe for
 *                           concurrent access from multiple threads.
 *   (default)             — std::shared_mutex (full thread-safety).
 *
 * Usage: -DAKASHA_SINGLE_THREAD (compiler flag) or via CMake option
 *        AKASHA_SINGLE_THREAD=ON.
 */

#ifndef AKASHA_SINGLE_THREAD
#include <shared_mutex>
#endif

namespace akasha::detail {

#ifdef AKASHA_SINGLE_THREAD

/**
 * @brief Drop-in no-op replacement for std::shared_mutex.
 *
 * Satisfies both BasicLockable and SharedMutex concepts so it can be used
 * with std::unique_lock<> and std::shared_lock<> without changes to call
 * sites. All operations are inlined no-ops — the compiler eliminates them
 * entirely in optimized builds.
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
