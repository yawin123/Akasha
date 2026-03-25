#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: Load/Unload Errors
// ============================================================================

TEST(loaderrors_unload_nonexistent_dataset) {
    akasha::Store store;
    auto status = store.unload("nonexistent");
    ASSERT_NE(status, akasha::Status::ok);
}

TEST(loaderrors_duplicate_load_same_dataset) {
    TempFile temp;
    akasha::Store store;
    auto status1 = store.load("dataset", temp.path(), akasha::FileOptions::create_if_missing);
    ASSERT_EQ(status1, akasha::Status::ok);
    auto status2 = store.load("dataset", temp.path(), akasha::FileOptions::none);
    // Cannot load the same dataset twice
    ASSERT_NE(status2, akasha::Status::ok);
}

TEST(loaderrors_load_same_file_different_ids) {
    TempFile temp;
    akasha::Store store;
    auto status1 = store.load("dataset1", temp.path(), akasha::FileOptions::create_if_missing);
    ASSERT_EQ(status1, akasha::Status::ok);
    // Cannot load the same file with a different ID
    auto status2 = store.load("dataset2", temp.path(), akasha::FileOptions::none);
    ASSERT_EQ(status2, akasha::Status::source_already_loaded);   
}
