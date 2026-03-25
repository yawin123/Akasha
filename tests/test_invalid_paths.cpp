#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: Invalid Path Formats
// ============================================================================

TEST(invalidpaths_double_dots_in_path) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    auto status = store.set<int>("data..key", 42);
    ASSERT_EQ(status, akasha::Status::invalid_key_path);
}

TEST(invalidpaths_path_starting_with_dot) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    auto status = store.set<int>("data.key", 42);
    ASSERT_EQ(status, akasha::Status::ok);  // Normal case
    // This requires dataset first
    auto status2 = store.set<int>(".key", 42);
    ASSERT_NE(status2, akasha::Status::ok);
}

TEST(invalidpaths_path_ending_with_dot) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    auto status = store.set<int>("data.key.", 42);
    ASSERT_EQ(status, akasha::Status::invalid_key_path);
}

TEST(invalidpaths_only_dot_as_path) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    auto status = store.set<int>(".", 42);
    ASSERT_EQ(status, akasha::Status::invalid_key_path);
}
