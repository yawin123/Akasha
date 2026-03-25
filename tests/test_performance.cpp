#include "test_framework.hpp"
#include "test_common.hpp"
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================================
// Tests: Performance Tuning - File Size Behavior
// ============================================================================

TEST(performance_initial_file_size_small) {
    TempFile temp;
    akasha::Store store;
    
    // Set small initial file size
    akasha::PerformanceTuning tuning;
    tuning.initial_mapped_file_size = 16 * 1024;      // 16 KB
    tuning.initial_grow_step = 8 * 1024;              // 8 KB growth
    tuning.max_grow_retries = 8;
    store.set_performance_tuning(tuning);
    
    // Load with custom tuning - should create 16 KB file
    auto status = store.load("test", temp.path(), akasha::FileOptions::create_if_missing);
    ASSERT_EQ(status, akasha::Status::ok);
    
    // File should be exactly initial_mapped_file_size bytes
    std::size_t initial_size = fs::file_size(temp.path());
    ASSERT_EQ(initial_size, 16 * 1024);
    
    (void)store.unload("test");
}

TEST(performance_initial_file_size_large) {
    TempFile temp;
    akasha::Store store;
    
    // Set larger initial file size
    akasha::PerformanceTuning tuning;
    tuning.initial_mapped_file_size = 256 * 1024;     // 256 KB
    tuning.initial_grow_step = 128 * 1024;            // 128 KB growth
    tuning.max_grow_retries = 8;
    store.set_performance_tuning(tuning);
    
    // Load with custom tuning
    auto status = store.load("test", temp.path(), akasha::FileOptions::create_if_missing);
    ASSERT_EQ(status, akasha::Status::ok);
    
    // File should be exactly initial_mapped_file_size bytes
    std::size_t initial_size = fs::file_size(temp.path());
    ASSERT_EQ(initial_size, 256 * 1024);
    
    (void)store.unload("test");
}

TEST(performance_file_growth_steps) {
    TempFile temp;
    akasha::Store store;
    
    // Set small file with precise growth steps
    akasha::PerformanceTuning tuning;
    tuning.initial_mapped_file_size = 32 * 1024;      // 32 KB initial
    tuning.initial_grow_step = 16 * 1024;             // 16 KB steps
    tuning.max_grow_retries = 10;
    store.set_performance_tuning(tuning);
    
    auto status = store.load("test", temp.path(), akasha::FileOptions::create_if_missing);
    ASSERT_EQ(status, akasha::Status::ok);
    
    std::size_t size_after_create = fs::file_size(temp.path());
    ASSERT_EQ(size_after_create, 32 * 1024);
    
    // Insert data that fits in initial size
    (void)store.set<std::int32_t>("test.value1", 100);
    std::size_t size_after_small_write = fs::file_size(temp.path());
    ASSERT_EQ(size_after_small_write, 32 * 1024);  // No growth needed
    
    // Insert larger data that requires growth
    std::string large_value(20000, 'X');  // ~20 KB
    (void)store.set<std::string>("test.large", large_value);
    std::size_t size_after_large_write = fs::file_size(temp.path());
    
    // File should have grown by at least one step (16 KB)
    ASSERT_TRUE(size_after_large_write > size_after_small_write);
    // Verify growth is proportional to grow_step
    ASSERT_TRUE(size_after_large_write >= 32 * 1024 + 16 * 1024);
    
    (void)store.unload("test");
}

TEST(performance_data_integrity_with_growth) {
    TempFile temp;
    akasha::Store store;
    
    // Use small file to force growth
    akasha::PerformanceTuning tuning;
    tuning.initial_mapped_file_size = 16 * 1024;
    tuning.initial_grow_step = 8 * 1024;
    tuning.max_grow_retries = 10;
    store.set_performance_tuning(tuning);
    
    auto status = store.load("test", temp.path(), akasha::FileOptions::create_if_missing);
    ASSERT_EQ(status, akasha::Status::ok);
    
    // Store test data
    (void)store.set<std::int32_t>("test.count", 42);
    (void)store.set<std::string>("test.name", "TestValue");
    (void)store.set<bool>("test.flag", true);
    
    // Insert large data to trigger growth
    std::string large_data(15000, 'D');  // Will require file growth
    (void)store.set<std::string>("test.bulk", large_data);
    
    // Verify all data is intact after growth
    auto count = store.get<std::int32_t>("test.count");
    ASSERT_TRUE(count.has_value());
    ASSERT_EQ(count.value(), 42);
    
    auto name = store.get<std::string>("test.name");
    ASSERT_TRUE(name.has_value());
    ASSERT_EQ(name.value(), "TestValue");
    
    auto flag = store.get<bool>("test.flag");
    ASSERT_TRUE(flag.has_value());
    ASSERT_EQ(flag.value(), true);
    
    auto bulk = store.get<std::string>("test.bulk");
    ASSERT_TRUE(bulk.has_value());
    ASSERT_EQ(bulk.value(), large_data);
    
    (void)store.unload("test");
}

TEST(performance_different_tunings_same_file) {
    TempFile temp;
    
    // ===== SESSION 1: Create file with small tuning =====
    {
        akasha::Store store1;
        akasha::PerformanceTuning tuning1;
        tuning1.initial_mapped_file_size = 32 * 1024;
        tuning1.initial_grow_step = 16 * 1024;
        tuning1.max_grow_retries = 8;
        store1.set_performance_tuning(tuning1);
        
        auto status = store1.load("data", temp.path(), akasha::FileOptions::create_if_missing);
        ASSERT_EQ(status, akasha::Status::ok);
        
        // Insert initial data
        (void)store1.set<std::int32_t>("data.session", 1);
        (void)store1.set<std::string>("data.text", "Session1Data");
        (void)store1.set<std::vector<int>>("data.numbers", {1, 2, 3, 4, 5});
        
        (void)store1.unload("data");
    }
    
    std::size_t size_after_session1 = fs::file_size(temp.path());
    
    // ===== SESSION 2: Reopen with medium tuning, add more data =====
    {
        akasha::Store store2;
        akasha::PerformanceTuning tuning2;
        tuning2.initial_mapped_file_size = 64 * 1024;   // Larger than session 1
        tuning2.initial_grow_step = 32 * 1024;
        tuning2.max_grow_retries = 6;
        store2.set_performance_tuning(tuning2);
        
        auto status = store2.load("data", temp.path(), akasha::FileOptions::none);
        ASSERT_EQ(status, akasha::Status::ok);
        
        // Verify session 1 data is intact
        auto session_num = store2.get<std::int32_t>("data.session");
        ASSERT_TRUE(session_num.has_value());
        ASSERT_EQ(session_num.value(), 1);
        
        auto text = store2.get<std::string>("data.text");
        ASSERT_TRUE(text.has_value());
        ASSERT_EQ(text.value(), "Session1Data");
        
        auto numbers = store2.get<std::vector<int>>("data.numbers");
        ASSERT_TRUE(numbers.has_value());
        ASSERT_EQ(numbers.value(), std::vector<int>({1, 2, 3, 4, 5}));
        
        // Add new data in session 2
        (void)store2.set<std::int32_t>("data.session2_count", 100);
        (void)store2.set<std::string>("data.session2_text", "NewDataFromSession2");
        
        (void)store2.unload("data");
    }
    
    std::size_t size_after_session2 = fs::file_size(temp.path());
    ASSERT_TRUE(size_after_session2 >= size_after_session1);  // File should not shrink
    
    // ===== SESSION 3: Reopen with large tuning, verify all data =====
    {
        akasha::Store store3;
        akasha::PerformanceTuning tuning3;
        tuning3.initial_mapped_file_size = 128 * 1024;
        tuning3.initial_grow_step = 64 * 1024;
        tuning3.max_grow_retries = 4;
        store3.set_performance_tuning(tuning3);
        
        auto status = store3.load("data", temp.path(), akasha::FileOptions::none);
        ASSERT_EQ(status, akasha::Status::ok);
        
        // Verify ALL data from all sessions
        auto session1 = store3.get<std::int32_t>("data.session");
        ASSERT_TRUE(session1.has_value());
        ASSERT_EQ(session1.value(), 1);
        
        auto text1 = store3.get<std::string>("data.text");
        ASSERT_TRUE(text1.has_value());
        ASSERT_EQ(text1.value(), "Session1Data");
        
        auto numbers1 = store3.get<std::vector<int>>("data.numbers");
        ASSERT_TRUE(numbers1.has_value());
        ASSERT_EQ(numbers1.value(), std::vector<int>({1, 2, 3, 4, 5}));
        
        auto session2_count = store3.get<std::int32_t>("data.session2_count");
        ASSERT_TRUE(session2_count.has_value());
        ASSERT_EQ(session2_count.value(), 100);
        
        auto session2_text = store3.get<std::string>("data.session2_text");
        ASSERT_TRUE(session2_text.has_value());
        ASSERT_EQ(session2_text.value(), "NewDataFromSession2");
        
        // Add more data in session 3
        (void)store3.set<std::int32_t>("data.session3_count", 200);
        (void)store3.set<std::string>("data.session3_text", "DataFromSession3");
        
        (void)store3.unload("data");
    }
    
    // ===== SESSION 4: Final verification =====
    {
        akasha::Store store4;
        // Use default tuning this time
        auto status = store4.load("data", temp.path(), akasha::FileOptions::none);
        ASSERT_EQ(status, akasha::Status::ok);
        
        // Verify data from all 3 sessions
        auto s1 = store4.get<std::int32_t>("data.session");
        ASSERT_TRUE(s1.has_value());
        ASSERT_EQ(s1.value(), 1);
        
        auto s2c = store4.get<std::int32_t>("data.session2_count");
        ASSERT_TRUE(s2c.has_value());
        ASSERT_EQ(s2c.value(), 100);
        
        auto s3c = store4.get<std::int32_t>("data.session3_count");
        ASSERT_TRUE(s3c.has_value());
        ASSERT_EQ(s3c.value(), 200);
        
        (void)store4.unload("data");
    }
}

