#include <chrono>
#include <cstdint>
#include <atomic>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>

#include "akasha.hpp"

#define TEST_TIME std::chrono::seconds(1)

size_t countKeysRecursive(const akasha::Store::DatasetView& view)
{
    size_t total = 0;

    for (const auto& key : view.keys()) {
        const auto entry = view.get(key);
        if (!entry.has_value()) {
            continue;
        }

        if (std::holds_alternative<akasha::ValueView>(*entry)) {
            ++total;
            continue;
        }

        total += countKeysRecursive(std::get<akasha::Store::DatasetView>(*entry));
    }

    return total;
}

size_t getResult(akasha::Store& store, std::string_view dataset = "test")
{
    const auto entry = store.get(dataset);
    if (!entry.has_value()) {
        std::cerr << "get failed: dataset view not found\n";
        return -1;
    }

    const auto* dataset_view = std::get_if<akasha::Store::DatasetView>(&*entry);
    if (dataset_view == nullptr) {
        std::cerr << "get failed: expected dataset view\n";
        return -1;
    }

    return countKeysRecursive(*dataset_view);
}

void clearStore(akasha::Store& store)
{
    const auto clear_status = store.clear();
    if (clear_status != akasha::WriteStatus::ok) {
        std::cerr << "clear failed with status: " << static_cast<int>(clear_status) << '\n';
    }
}

size_t test1(akasha::Store& store)
{
    std::uint64_t initial_value = 123456;

    std::uint64_t counter = 0;
    const auto end_time = std::chrono::steady_clock::now() + TEST_TIME;

    while (std::chrono::steady_clock::now() <= end_time ) {
        ++counter;

        const std::string actual_key = "test.1." + std::to_string(counter);
        const std::string last_key = "test.1." + std::to_string(counter - 1);

        std::uint64_t value_to_store = initial_value;
        if (counter > 1) {
            const auto last_value = store.get<std::uint64_t>(last_key);
            if (!last_value.has_value()) {
                std::cerr << "get failed for last_key: " << last_key << '\n';
                return 1;
            }
            value_to_store = *last_value;
        }

        const akasha::WriteStatus set_status = store.set<std::uint64_t>(actual_key, value_to_store);
        if (set_status != akasha::WriteStatus::ok) {
            std::cerr << "set failed with status: " << static_cast<int>(set_status) << '\n';
            return 1;
        }
    }

    return getResult(store, "test.1");
}

size_t test2(akasha::Store& store)
{
    clearStore(store);

    std::atomic<bool> stop{false};
    std::atomic<bool> failed{false};

    const auto end_time = std::chrono::steady_clock::now() + TEST_TIME;

    auto worker = [&](const std::string& prefix) {
        std::uint64_t local_counter = 0;

        while (!stop.load(std::memory_order_relaxed)) {
            if (std::chrono::steady_clock::now() >= end_time) {
                stop.store(true, std::memory_order_relaxed);
                break;
            }

            ++local_counter;
            const std::string key = prefix + "." + std::to_string(local_counter);

            const akasha::WriteStatus set_status = store.set<std::uint64_t>(key, local_counter);
            if (set_status != akasha::WriteStatus::ok) {
                std::cerr << "set failed in thread for prefix " << prefix
                          << " with status: " << static_cast<int>(set_status) << '\n';
                failed.store(true, std::memory_order_relaxed);
                stop.store(true, std::memory_order_relaxed);
                break;
            }
        }
    };

    std::thread hilo1(worker, "test.2.hilo1");
    std::thread hilo2(worker, "test.2.hilo2");

    hilo1.join();
    hilo2.join();

    if (failed.load(std::memory_order_relaxed)) {
        return static_cast<size_t>(-1);
    }

    return getResult(store, "test.2");
}

size_t test3(akasha::Store& store)
{
    clearStore(store);

    constexpr std::size_t key_count = 10;
    std::vector<std::string> keys;
    std::vector<std::uint64_t> expected_values;
    keys.reserve(key_count);
    expected_values.reserve(key_count);

    std::random_device rd;
    std::mt19937_64 rng(rd());
    std::uniform_int_distribution<std::uint64_t> value_dist(1, 1'000'000'000ULL);

    for (std::size_t index = 0; index < key_count; ++index) {
        keys.push_back("test.3.rand." + std::to_string(index));
        expected_values.push_back(value_dist(rng));

        const akasha::WriteStatus set_status = store.set<std::uint64_t>(keys.back(), expected_values.back());
        if (set_status != akasha::WriteStatus::ok) {
            std::cerr << "set failed in test3 init with status: " << static_cast<int>(set_status) << '\n';
            return static_cast<size_t>(-1);
        }
    }

    std::uniform_int_distribution<std::size_t> key_index_dist(0, key_count - 1);
    std::size_t success_counter = 0;
    const auto end_time = std::chrono::steady_clock::now() + TEST_TIME;

    while (std::chrono::steady_clock::now() <= end_time) {
        const std::size_t index = key_index_dist(rng);
        const auto value = store.get<std::uint64_t>(keys[index]);
        if (!value.has_value()) {
            continue;
        }

        if (*value == expected_values[index]) {
            ++success_counter;
        }
    }

    return success_counter;
}

int main() {
    akasha::Store store;

    akasha::PerformanceTuning tuning;
    tuning.initial_mapped_file_size = 64 * 1024;
    tuning.initial_grow_step = tuning.initial_mapped_file_size / 2;
    tuning.max_grow_retries = 8;
    store.set_performance_tuning(tuning);

    std::string file_path = "build/test_loop.mmap";
    std::filesystem::remove(file_path);  // Limpiar archivo previo para pruebas consistentes

    const akasha::LoadStatus load_status = store.load("test", file_path, true);
    if (load_status != akasha::LoadStatus::ok) {
        std::cerr << "load failed with status: " << static_cast<int>(load_status) << '\n';
        return 1;
    }

    const size_t total_data_test1 = test1(store);
    std::cout << "Total entries stored (test1): " << total_data_test1 << '\n';

    const size_t total_data_test2 = test2(store);
    std::cout << "Total entries stored (test2): " << total_data_test2 << '\n';

    const size_t total_hits_test3 = test3(store);
    std::cout << "Total entries readed (test3): " << total_hits_test3 << '\n';

    const akasha::WriteStatus compact_status = store.compact("test");
    std::cout << "Compact status: " << static_cast<int>(compact_status) << '\n';

    return 0;
}
