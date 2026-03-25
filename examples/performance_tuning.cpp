/**
 * @brief Example: Performance Tuning Configuration
 * 
 * This example demonstrates how to configure Akasha's performance parameters
 * using the PerformanceTuning structure. These settings control file growth
 * behavior and memory allocation strategies.
 * 
 * Parameters explained:
 * - initial_mapped_file_size: Initial size of the memory-mapped file (bytes)
 * - initial_grow_step: Amount to grow the file when it runs out of space (bytes)
 * - max_grow_retries: Maximum retry attempts before returning file_full error
 */

#include <akasha.hpp>
#include <iostream>
#include <filesystem>
#include <iomanip>
#include <chrono>

namespace fs = std::filesystem;

void print_tuning(const std::string& label, const akasha::PerformanceTuning& tuning) {
    std::cout << "\n" << label << ":\n";
    std::cout << "  initial_mapped_file_size:  " << std::setw(8) << tuning.initial_mapped_file_size / 1024 << " KB\n";
    std::cout << "  initial_grow_step:         " << std::setw(8) << tuning.initial_grow_step / 1024 << " KB\n";
    std::cout << "  max_grow_retries:          " << std::setw(8) << tuning.max_grow_retries << "\n";
}

int main() {
    std::cout << "\n=== Akasha Performance Tuning Configuration ===\n";

    // Create a Store and check default tuning parameters
    akasha::Store store;
    auto default_tuning = store.performance_tuning();
    print_tuning("Default Performance Tuning", default_tuning);

    // ==================== SCENARIO 1: SMALL FILES ====================
    std::cout << "\n\n--- Scenario 1: Small files (IoT sensors, embedded systems) ---\n";
    
    akasha::PerformanceTuning small_file_tuning{
        .initial_mapped_file_size = 8 * 1024,      // 8 KB (start very small)
        .initial_grow_step = 4 * 1024,             // 4 KB (grow by small steps)
        .max_grow_retries = 12                     // More retries for gradual growth
    };
    
    store.set_performance_tuning(small_file_tuning);
    print_tuning("Small File Tuning", store.performance_tuning());
    std::cout << "  Use case: Storing sensor readings, device settings\n";
    std::cout << "  Benefit: Minimal memory footprint\n";

    // ==================== SCENARIO 2: MEDIUM FILES ====================
    std::cout << "\n\n--- Scenario 2: Medium files (Web services, config servers) ---\n";
    
    akasha::PerformanceTuning medium_file_tuning{
        .initial_mapped_file_size = 256 * 1024,    // 256 KB
        .initial_grow_step = 128 * 1024,           // 128 KB (balanced growth)
        .max_grow_retries = 8                      // Standard retries
    };
    
    store.set_performance_tuning(medium_file_tuning);
    print_tuning("Medium File Tuning", store.performance_tuning());
    std::cout << "  Use case: REST API configuration, user settings\n";
    std::cout << "  Benefit: Good balance between memory and growth efficiency\n";

    // ==================== SCENARIO 3: LARGE FILES ====================
    std::cout << "\n\n--- Scenario 3: Large files (Databases, analytics) ---\n";
    
    akasha::PerformanceTuning large_file_tuning{
        .initial_mapped_file_size = 4 * 1024 * 1024,    // 4 MB
        .initial_grow_step = 2 * 1024 * 1024,           // 2 MB (aggressive growth)
        .max_grow_retries = 5                           // Fewer retries (fewer small grows)
    };
    
    store.set_performance_tuning(large_file_tuning);
    print_tuning("Large File Tuning", store.performance_tuning());
    std::cout << "  Use case: Analytics data, time-series metrics\n";
    std::cout << "  Benefit: Reduced overhead from frequent file growth\n";

    // ==================== DEMONSTRATION ====================
    std::cout << "\n\n--- Practical Demonstration ---\n";
    std::cout << "Creating a store with small file tuning and inserting data...\n\n";

    // Reset to small file tuning
    store.set_performance_tuning(small_file_tuning);

    std::string db_path = "/tmp/akasha_perf_tuning_demo.db";
    if (fs::exists(db_path)) {
        fs::remove(db_path);
    }

    // Load dataset with small file tuning
    auto load_status = store.load("settings", db_path, akasha::FileOptions::create_if_missing);
    if (load_status != akasha::Status::ok) {
        std::cerr << "Error loading settings: " << static_cast<int>(load_status) << "\n";
        return 1;
    }

    std::cout << "Initial file size: " << fs::file_size(db_path) / 1024 << " KB\n";

    // Insert small values
    std::cout << "Inserting configuration values...\n";
    store.set<std::int32_t>("settings.app.max_connections", 100);
    store.set<std::int32_t>("settings.app.timeout_seconds", 30);
    store.set<std::string>("settings.app.log_level", "INFO");
    store.set<bool>("settings.app.enable_cache", true);

    std::cout << "After small inserts: " << fs::file_size(db_path) / 1024 << " KB\n";

    // Insert larger data to trigger growth
    std::cout << "Inserting larger data (will trigger file growth)...\n";
    std::string large_value(2048, 'X');  // 2 KB string
    for (int i = 0; i < 5; ++i) {
        auto key = "settings.data.chunk_" + std::to_string(i);
        store.set<std::string>(key, large_value);
        std::cout << "  After chunk " << i << ": " << fs::file_size(db_path) / 1024 << " KB\n";
    }

    store.unload("settings");

    std::cout << "\n✓ Final file size: " << fs::file_size(db_path) / 1024 << " KB\n";
    std::cout << "\nNotice how the file grew in steps according to initial_grow_step.\n";

    // ==================== RETRIEVE AND VERIFY TUNING ====================
    std::cout << "\n--- Verifying stored configuration ---\n";
    
    auto verified_tuning = store.performance_tuning();
    print_tuning("Current Store Tuning", verified_tuning);

    std::cout << "\n✓ You can query the current tuning at any time with performance_tuning()\n";
    std::cout << "✓ Changes apply to new datasets or at the next file growth event\n";

    // ==================== CLEANUP ====================
    fs::remove(db_path);
    std::cout << "\nCleanup: Removed temporary file\n";

    std::cout << "\n=== Key Takeaways ===\n";
    std::cout << "1. Use set_performance_tuning() to customize file growth behavior\n";
    std::cout << "2. PerformanceTuning has 3 parameters: size, growth step, retry count\n";
    std::cout << "3. Tune based on your use case: small embedded, medium web, or large analytics\n";
    std::cout << "4. Query current tuning with performance_tuning() at any time\n";
    std::cout << "5. Different applications may need different strategies\n\n";

    return 0;
}
