#include <iostream>
#include <chrono>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <string>
#include "akasha.hpp"

namespace fs = std::filesystem;

struct BenchmarkResult {
    std::string name;
    double time_ms{0.0};
    size_t count{0};
    
    double ops_per_sec() const {
        if (time_ms == 0) return 0;
        return (count * 1000.0) / time_ms;
    }
};

void print_result(const BenchmarkResult& result) {
    std::cout << std::left << std::setw(50) << result.name 
              << std::right << std::setw(12) << std::fixed << std::setprecision(3) << result.time_ms << " ms"
              << std::setw(15) << std::setprecision(0) << result.ops_per_sec() << " ops/sec\n";
}

// Benchmark 1: Load dataset
BenchmarkResult benchmark_load(const std::string& path) {
    // Clean up previous file
    fs::remove(path);
    
    auto start = std::chrono::high_resolution_clock::now();
    akasha::Store store;
    (void)store.load("bench", path, true);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();
    
    fs::remove(path);
    
    return {"Load dataset (empty)", duration, 1};
}

// Benchmark 2: Sequential writes (scalar)
BenchmarkResult benchmark_write_scalar(size_t count) {
    fs::remove("/tmp/bench_scalar.db");
    akasha::Store store;
    (void)store.load("bench", "/tmp/bench_scalar.db", true);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < count; ++i) {
        (void)store.set<int64_t>("bench.value." + std::to_string(i), static_cast<int64_t>(i));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();
    
    fs::remove("/tmp/bench_scalar.db");
    
    return {"Write scalar (" + std::to_string(count) + " keys)", duration, count};
}

// Benchmark 3: Sequential reads (scalar)
BenchmarkResult benchmark_read_scalar(size_t count) {
    fs::remove("/tmp/bench_scalar.db");
    akasha::Store store;
    (void)store.load("bench", "/tmp/bench_scalar.db", true);
    
    // Pre-populate
    for (size_t i = 0; i < count; ++i) {
        (void)store.set<int64_t>("bench.value." + std::to_string(i), static_cast<int64_t>(i));
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    size_t found = 0;
    for (size_t i = 0; i < count; ++i) {
        auto val = store.get<int64_t>("bench.value." + std::to_string(i));
        if (val.has_value()) ++found;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();
    
    fs::remove("/tmp/bench_scalar.db");
    
    return {"Read scalar (" + std::to_string(count) + " keys, " + std::to_string(found) + " found)", duration, count};
}

// Benchmark 4: Sequential writes (string)
BenchmarkResult benchmark_write_string(size_t count) {
    fs::remove("/tmp/bench_string.db");
    akasha::Store store;
    (void)store.load("bench", "/tmp/bench_string.db", true);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < count; ++i) {
        std::string value = "Value number " + std::to_string(i) + " with some extra padding";
        (void)store.set<std::string>("bench.str." + std::to_string(i), value);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();
    
    fs::remove("/tmp/bench_string.db");
    
    return {"Write string (" + std::to_string(count) + " keys)", duration, count};
}

// Benchmark 5: Sequential reads (string)
BenchmarkResult benchmark_read_string(size_t count) {
    fs::remove("/tmp/bench_string.db");
    akasha::Store store;
    (void)store.load("bench", "/tmp/bench_string.db", true);
    
    // Pre-populate
    for (size_t i = 0; i < count; ++i) {
        std::string value = "Value number " + std::to_string(i) + " with some extra padding";
        (void)store.set<std::string>("bench.str." + std::to_string(i), value);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    size_t found = 0;
    for (size_t i = 0; i < count; ++i) {
        auto val = store.get<std::string>("bench.str." + std::to_string(i));
        if (val.has_value()) ++found;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();
    
    fs::remove("/tmp/bench_string.db");
    
    return {"Read string (" + std::to_string(count) + " keys, " + std::to_string(found) + " found)", duration, count};
}

// Benchmark 6: Struct writes
struct Point { double x, y, z; };

BenchmarkResult benchmark_write_struct(size_t count) {
    fs::remove("/tmp/bench_struct.db");
    akasha::Store store;
    (void)store.load("bench", "/tmp/bench_struct.db", true);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < count; ++i) {
        Point p{static_cast<double>(i), static_cast<double>(i) * 2, static_cast<double>(i) * 3};
        (void)store.set<Point>("bench.point." + std::to_string(i), p);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();
    
    fs::remove("/tmp/bench_struct.db");
    
    return {"Write struct (" + std::to_string(count) + " keys)", duration, count};
}

// Benchmark 7: Struct reads
BenchmarkResult benchmark_read_struct(size_t count) {
    fs::remove("/tmp/bench_struct.db");
    akasha::Store store;
    (void)store.load("bench", "/tmp/bench_struct.db", true);
    
    // Pre-populate
    for (size_t i = 0; i < count; ++i) {
        Point p{static_cast<double>(i), static_cast<double>(i) * 2, static_cast<double>(i) * 3};
        (void)store.set<Point>("bench.point." + std::to_string(i), p);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    size_t found = 0;
    for (size_t i = 0; i < count; ++i) {
        auto val = store.get<Point>("bench.point." + std::to_string(i));
        if (val.has_value()) ++found;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();
    
    fs::remove("/tmp/bench_struct.db");
    
    return {"Read struct (" + std::to_string(count) + " keys, " + std::to_string(found) + " found)", duration, count};
}

// Benchmark 8: Compact effect
BenchmarkResult benchmark_compact(size_t count) {
    fs::remove("/tmp/bench_compact.db");
    akasha::Store store;
    (void)store.load("bench", "/tmp/bench_compact.db", true);
    
    // Write data
    for (size_t i = 0; i < count; ++i) {
        auto status = store.set<int64_t>("bench.val." + std::to_string(i), static_cast<int64_t>(i));
        (void)status;
    }
    
    // Clear half
    for (size_t i = 0; i < count / 2; ++i) {
        auto status = store.clear("bench.val." + std::to_string(i));
        (void)status;
    }
    
    // Measure compact time
    auto start = std::chrono::high_resolution_clock::now();
    auto compact_status = store.compact("bench");
    (void)compact_status;
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();
    
    fs::remove("/tmp/bench_compact.db");
    
    return {"Compact after 50% delete (" + std::to_string(count) + " keys)", duration, 1};
}

int main() {
    std::cout << "\n=== Akasha Benchmarks ===\n\n";
    
    std::cout << std::left << std::setw(50) << "Operation"
              << std::right << std::setw(12) << "Time"
              << std::setw(15) << "Throughput\n"
              << std::string(77, '-') << '\n';
    
    // Basic operations
    print_result(benchmark_load("/tmp/bench_load.db"));
    std::cout << '\n';
    
    // Scalars
    print_result(benchmark_write_scalar(1000));
    print_result(benchmark_read_scalar(1000));
    std::cout << '\n';
    
    print_result(benchmark_write_scalar(10000));
    print_result(benchmark_read_scalar(10000));
    std::cout << '\n';
    
    // Strings
    print_result(benchmark_write_string(1000));
    print_result(benchmark_read_string(1000));
    std::cout << '\n';
    
    print_result(benchmark_write_string(10000));
    print_result(benchmark_read_string(10000));
    std::cout << '\n';
    
    // Structs
    print_result(benchmark_write_struct(1000));
    print_result(benchmark_read_struct(1000));
    std::cout << '\n';
    
    print_result(benchmark_write_struct(10000));
    print_result(benchmark_read_struct(10000));
    std::cout << '\n';
    
    // Compaction
    print_result(benchmark_compact(5000));
    print_result(benchmark_compact(10000));
    std::cout << '\n';
    
    return 0;
}
