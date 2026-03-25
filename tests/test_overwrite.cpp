#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: Overwriting Values
// ============================================================================

TEST(overwrite_int64) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    // Initial set
    (void)store.set<std::int64_t>("data.value", 42);
    auto retrieved1 = store.get<std::int64_t>("data.value");
    ASSERT_EQ(retrieved1.value(), 42);
    // Overwrite
    (void)store.set<std::int64_t>("data.value", 99);
    auto retrieved2 = store.get<std::int64_t>("data.value");
    ASSERT_EQ(retrieved2.value(), 99);
}

TEST(overwrite_string) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    // Initial set
    (void)store.set<std::string>("data.text", "hello");
    auto retrieved1 = store.get<std::string>("data.text");
    ASSERT_STREQ(retrieved1.value(), "hello");
    // Overwrite
    (void)store.set<std::string>("data.text", "goodbye");
    auto retrieved2 = store.get<std::string>("data.text");
    ASSERT_STREQ(retrieved2.value(), "goodbye");
}

TEST(overwrite_with_different_sizes) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    // Small
    (void)store.set<std::string>("data.text", "hi");
    ASSERT_STREQ(store.get<std::string>("data.text").value(), "hi");
    // Large
    (void)store.set<std::string>("data.text", "this is a much longer string that takes more space");
    ASSERT_EQ(store.get<std::string>("data.text").value(), "this is a much longer string that takes more space");
    // Small again
    (void)store.set<std::string>("data.text", "x");
    ASSERT_STREQ(store.get<std::string>("data.text").value(), "x");
}
