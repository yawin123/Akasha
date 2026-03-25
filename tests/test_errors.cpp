#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: Status and Error Handling
// ============================================================================

TEST(errors_invalid_key_path_error) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    // Empty key path should fail with invalid_key_path error
    auto status = store.set<int>("", 42);
    ASSERT_EQ(status, akasha::Status::invalid_key_path);
}

TEST(errors_dataset_not_found_error) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    // Access unloaded dataset
    auto result = store.get<std::int64_t>("nonexistent.key");
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(store.last_status(), akasha::Status::dataset_not_found);
}

TEST(errors_key_not_found_error) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    // Dataset exists but key doesn't
    auto result = store.get<std::int64_t>("data.nonexistent");
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(store.last_status(), akasha::Status::key_not_found);
}
