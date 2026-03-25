#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: Strings
// ============================================================================

TEST(strings_set_get_string) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    std::string original = "Hello, Akasha!";
    auto set_status = store.set<std::string>("data.message", original);
    ASSERT_EQ(set_status, akasha::Status::ok);
    ASSERT_TRUE(store.has("data.message"));
    auto retrieved = store.get<std::string>("data.message");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_STREQ(retrieved.value(), original);
}

TEST(strings_set_get_empty_string) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    std::string original = "";
    auto set_status = store.set<std::string>("data.empty", original);
    ASSERT_EQ(set_status, akasha::Status::ok);
    ASSERT_TRUE(store.has("data.empty"));
    auto retrieved = store.get<std::string>("data.empty");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_STREQ(retrieved.value(), original);
}

TEST(strings_set_get_unicode_string) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    std::string original = "你好世界 🌍";
    auto set_status = store.set<std::string>("data.unicode", original);
    ASSERT_EQ(set_status, akasha::Status::ok);
    ASSERT_TRUE(store.has("data.unicode"));
    auto retrieved = store.get<std::string>("data.unicode");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_STREQ(retrieved.value(), original);
}

TEST(strings_size_transitions) {
    TempFile temp;
    akasha::Store store;
    
    // Use small initial size to see fragmentation clearly
    akasha::PerformanceTuning small_tuning{
        .initial_mapped_file_size = 1024,  // 1KB
        .initial_grow_step = 2048,         // 2KB
        .max_grow_retries = 8
    };
    store.set_performance_tuning(small_tuning);
    
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    auto size_after_create = fs::file_size(temp.path());
    
    // Write small string
    ASSERT_EQ(store.set<std::string>("data.text", "small"), akasha::Status::ok);
    ASSERT_STREQ(store.get<std::string>("data.text").value(), "small");
    auto size_after_small = fs::file_size(temp.path());
    
    // Write large string
    std::string large_text(10000, 'L');  // 10 KB string
    size_t large_text_size = large_text.size();

    ASSERT_EQ(store.set<std::string>("data.text", large_text), akasha::Status::ok);
    ASSERT_SIZE(store.get<std::string>("data.text").value(), large_text_size);
    auto size_after_large = fs::file_size(temp.path());
    
    // Write empty string (file still doesn't shrink)
    ASSERT_EQ(store.set<std::string>("data.text", ""), akasha::Status::ok);
    ASSERT_EMPTY(store.get<std::string>("data.text").value());
    auto size_after_empty = fs::file_size(temp.path());
    
    // Compact to reclaim fragmented space
    ASSERT_EQ(store.compact("data"), akasha::Status::ok);
    auto size_after_compact = fs::file_size(temp.path());
    
    // Compact must reduce file size
    ASSERT_EQ(size_after_create, 1024);
    ASSERT_EQ(size_after_small, size_after_create);
    ASSERT_LT(size_after_small, size_after_large);
    ASSERT_EQ(size_after_empty, size_after_large); 
    ASSERT_LT(size_after_compact, size_after_empty);
}
