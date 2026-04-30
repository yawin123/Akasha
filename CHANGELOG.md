# Changelog

All notable changes to Akasha are documented in this file.

## [2.0.0] - 2026-04-30

### BREAKING CHANGES

- **Binary Format Incompatibility**: v2.0.0 introduces a new binary format that is **not compatible with v1.x files**
  - When loading a v1.x file with v2.0.0, a backup copy (`.bak`) is created and a new empty v2.0.0 file is initialized
  - **No automatic migration**: v1.x data is not recovered. Users must manually export data from v1.x and re-import to v2.0.0
  - Reason: Wire format changed fundamentally; old data (stored without type tags) cannot be reliably interpreted in the new schema

### Added

- **Thread Safety Configuration**: New compile-time `AKASHA_THREAD_SAFE` flag for controlling multi-threaded access
  - Default (`OFF`): Single-threaded mode with zero locking overhead (constexpr no-op mutexes)
  - `ON`: Multi-threaded safe mode using `std::shared_mutex` for exclusive and shared locking
  - All source files updated to use abstraction layer in `include/akasha/detail/mutex.hpp`
  - Enables single-threaded performance optimization while maintaining multi-threaded capability

- **Performance Micro-optimizations**:
  - Optimized `clear_no_lock()`: Changed from O(n) full-scan to O(k log n) lower_bound-based algorithm
  - Removed unnecessary `flush()` from `set_bytes_impl()` hot path (kept only in `clear()` where semantically necessary)

### Changed

- **Benchmark Suite**: Comprehensive performance measurements across primitive types, simple serializable types, and complex nested structures
  - 300 iterations per test, 4 threads for heavy loads
  - Covers scalar keys, strings, vectors, and custom types (Point, Scene)
  - Results exported to `akasha_benchmark.csv` for analysis

- **Documentation**: Updated API reference and migration guidance
  - Clarified v1.x → v2.0.0 incompatibility and backup behavior
  - Added thread safety configuration examples
  - Documented performance characteristics and trade-offs

### Fixed

- Build failure caused by missing Git submodule initialization in Conan
- Benchmark table format and consistency

### Performance Characteristics

**Default Configuration (AKASHA_THREAD_SAFE=OFF)**:
- Write scalar keys (1K): 686K ops/s
- Read scalar keys (1K): 1.7M ops/s
- Write vectors (1K, 50 elem): 11K ops/s
- Read vectors (1K, 50 elem): 48K ops/s
- Write complex types (1K): 23K ops/s
- Read complex types (1K): 1.5M ops/s

**Lock Overhead (AKASHA_THREAD_SAFE=ON)**:
- Scalar writes: +15.3%
- Scalar reads: +4.6%
- Complex type reads: +94.6% (per-element locking in vectors)

---

## [1.1.0] - Previous Release

See git history for v1.x changes.
