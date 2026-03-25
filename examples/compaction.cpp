/**
 * @brief Example: File Compaction and Fragmentation Recovery
 * 
 * This example demonstrates how Akasha's compact() operation eliminates
 * internal fragmentation caused by overwriting values with smaller ones.
 * 
 * Scenario:
 * - Insert 1000 large strings
 * - Delete 90% of them (creating fragmentation)
 * - Measure file size before and after compaction
 * - Verify that compact() reclaims the wasted space
 */

#include <akasha.hpp>
#include <iostream>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

int main() {
    std::cout << "\n=== Akasha Compaction Example ===\n\n";

    // Create a temporary file for this example
    std::string db_path = "/tmp/akasha_compaction_demo.db";
    if (fs::exists(db_path)) {
        fs::remove(db_path);
    }

    akasha::Store store;

    // ==================== STEP 1: INSERT LARGE DATA ====================
    std::cout << "Step 1: Inserting 1000 large strings...\n";
    
    auto status = store.load("config", db_path, akasha::FileOptions::create_if_missing);
    if (status != akasha::Status::ok) {
        std::cerr << "Error loading dataset\n";
        return 1;
    }

    // Insert 1000 strings, each ~200 bytes
    for (int i = 0; i < 1000; ++i) {
        std::string key = "config.data." + std::to_string(i);
        std::string value = "This is a large value #" + std::to_string(i) + 
                           " with padding to make it approximately 200 bytes long. " +
                           "Lorem ipsum dolor sit amet, consectetur adipiscing elit. " +
                           "Sed do eiusmod tempor incididunt ut labore et dolore magna.";
        
        store.set<std::string>(key, value);
    }

    store.unload("config");
    std::size_t size_after_insert = fs::file_size(db_path);
    std::cout << "  Inserted 1000 large strings\n";
    std::cout << "  File size: " << size_after_insert / 1024 << " KB\n\n";

    // ==================== STEP 2: DELETE 90% OF DATA ====================
    std::cout << "Step 2: Deleting 90% of the data (900 out of 1000)...\n";
    
    status = store.load("config", db_path);
    if (status != akasha::Status::ok) {
        std::cerr << "Error reloading dataset\n";
        return 1;
    }

    // Delete 900 keys, keep only 100
    for (int i = 0; i < 900; ++i) {
        std::string key = "config.data." + std::to_string(i);
        store.clear(key);
    }

    store.unload("config");
    std::size_t size_after_delete = fs::file_size(db_path);
    std::cout << "  Deleted 900 keys, kept 100\n";
    std::cout << "  File size: " << size_after_delete / 1024 << " KB\n";
    std::cout << "  ⚠️  No space reclaimed yet (internal fragmentation)\n\n";

    // ==================== STEP 3: COMPACT THE FILE ====================
    std::cout << "Step 3: Running compact() to eliminate fragmentation...\n";
    
    status = store.load("config", db_path);
    if (status != akasha::Status::ok) {
        std::cerr << "Error reloading dataset\n";
        return 1;
    }

    // Measure time for compact operation
    auto start = std::chrono::high_resolution_clock::now();
    status = store.compact("config");
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    if (status != akasha::Status::ok) {
        std::cerr << "Error during compaction\n";
        return 1;
    }

    store.unload("config");
    std::size_t size_after_compact = fs::file_size(db_path);
    
    std::cout << "  Compaction completed in " << std::fixed << std::setprecision(2) 
              << duration_ms << " ms\n";
    std::cout << "  File size: " << size_after_compact / 1024 << " KB\n\n";

    // ==================== STEP 4: ANALYZE RESULTS ====================
    std::cout << "Results Summary:\n";
    std::cout << "  After insert (1000 items):  " << std::setw(8) << size_after_insert / 1024 << " KB\n";
    std::cout << "  After delete (100 items):   " << std::setw(8) << size_after_delete / 1024 << " KB (fragmented)\n";
    std::cout << "  After compact:              " << std::setw(8) << size_after_compact / 1024 << " KB (defragmented)\n";
    
    std::int64_t wasted_space = size_after_delete - size_after_compact;
    double waste_pct = (double)wasted_space / size_after_delete * 100;
    
    std::cout << "\n  Fragmented space reclaimed: " << wasted_space / 1024 << " KB (" 
              << std::fixed << std::setprecision(1) << waste_pct << "%)\n";

    // ==================== STEP 5: VERIFY DATA INTEGRITY ====================
    std::cout << "\nStep 4: Verifying data integrity after compact...\n";
    
    status = store.load("config", db_path);
    if (status != akasha::Status::ok) {
        std::cerr << "Error reloading dataset\n";
        return 1;
    }

    // Verify that the remaining 100 keys are intact (items 900-999)
    int verified = 0;
    for (int i = 900; i < 1000; ++i) {
        std::string key = "config.data." + std::to_string(i);
        auto value = store.get<std::string>(key);
        if (value.has_value()) {
            ++verified;
        }
    }

    std::cout << "  Verified " << verified << "/100 remaining keys\n";
    
    if (verified == 100) {
        std::cout << "  ✓ All data preserved after compaction\n";
    } else {
        std::cerr << "  ✗ Data integrity check failed!\n";
    }

    store.unload("config");

    // ==================== CLEANUP ====================
    fs::remove(db_path);
    std::cout << "\nCleanup: Removed temporary file\n";

    std::cout << "\n=== Key Takeaways ===\n";
    std::cout << "1. Overwriting with smaller values leaves fragmented space\n";
    std::cout << "2. compact() uses extract→delete→recreate strategy\n";
    std::cout << "3. Use compact() periodically for applications with high churn\n";
    std::cout << "4. compact() is thread-safe and preserves all data\n\n";

    return 0;
}
