#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: Dataset Root Values
// ============================================================================

TEST(rootvalues_dataset_root_value) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("config", temp.path(), akasha::FileOptions::create_if_missing);
    // Set at root (no .key suffix)
    auto status = store.set<std::int64_t>("config", 42);
    ASSERT_EQ(status, akasha::Status::ok);
    auto retrieved = store.get<std::int64_t>("config");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved.value(), 42);
}

TEST(rootvalues_dataset_root_overwrite) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    (void)store.set<std::int64_t>("data", 10);
    auto retrieved1 = store.get<std::int64_t>("data");
    ASSERT_EQ(retrieved1.value(), 10);
    (void)store.set<std::int64_t>("data", 20);
    auto retrieved2 = store.get<std::int64_t>("data");
    ASSERT_EQ(retrieved2.value(), 20);
}

TEST(rootvalues_dataset_root_value_and_child_coexistence) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("config", temp.path(), akasha::FileOptions::create_if_missing);
    // Set root value
    (void)store.set<std::int64_t>("config", 100);
    // Set child values
    (void)store.set<int>("config.timeout", 30);
    (void)store.set<std::string>("config.name", "myapp");
    // Verify root
    auto root = store.get<std::int64_t>("config");
    ASSERT_TRUE(root.has_value());
    ASSERT_EQ(root.value(), 100);
    // Verify children
    auto timeout = store.get<int>("config.timeout");
    ASSERT_TRUE(timeout.has_value());
    ASSERT_EQ(timeout.value(), 30);
    auto name = store.get<std::string>("config.name");
    ASSERT_TRUE(name.has_value());
    ASSERT_STREQ(name.value(), "myapp");
}
