#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: Basic Load/Unload and Persistence
// ============================================================================

TEST(loadunload_load_creates_file) {
    TempFile temp;
    akasha::Store store;
    // File should not exist before load
    ASSERT_FALSE(fs::exists(temp.path()));
    auto status = store.load("dataset1", temp.path(), akasha::FileOptions::create_if_missing);
    ASSERT_EQ(status, akasha::Status::ok);
    // File should exist after load
    ASSERT_TRUE(fs::exists(temp.path()));
}

TEST(loadunload_load_nonexistent_file_fails) {
    TempFile temp;
    ASSERT_FALSE(fs::exists(temp.path()));
    akasha::Store store;
    auto status = store.load("dataset1", temp.path(), akasha::FileOptions::none);
    ASSERT_EQ(status, akasha::Status::file_not_found);
}

TEST(loadunload_unload_removes_dataset) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("dataset1", temp.path(), akasha::FileOptions::create_if_missing);
    (void)store.set<int>("dataset1.key", 42);
    ASSERT_TRUE(store.has("dataset1.key"));
    ASSERT_EQ(store.get<int>("dataset1.key").value(), 42);
    auto status = store.unload("dataset1");
    ASSERT_EQ(status, akasha::Status::ok);
    ASSERT_FALSE(store.has("dataset1.key"));
}

TEST(loadunload_data_persists_after_unload) {
    TempFile temp;
    int value = 999;
    // First session: write data
    {
        akasha::Store store1;
        auto status = store1.load("data", temp.path(), akasha::FileOptions::create_if_missing);
        ASSERT_EQ(status, akasha::Status::ok);
        auto set_status = store1.set<int>("data.value", value);
        ASSERT_EQ(set_status, akasha::Status::ok);
        auto unload_status = store1.unload("data");
        ASSERT_EQ(unload_status, akasha::Status::ok);
    }
    // Second session: reopen and verify
    {
        akasha::Store store2;
        auto status = store2.load("data", temp.path(), akasha::FileOptions::none);
        ASSERT_EQ(status, akasha::Status::ok);
        auto retrieved = store2.get<int>("data.value");
        ASSERT_TRUE(retrieved.has_value());
        ASSERT_EQ(retrieved.value(), value);
    }
}

TEST(loadunload_multiple_datasets_in_store) {
    TempFile temp1("test_app.mmap"), temp2("test_config.mmap");
    akasha::Store store;
    int app_version = 1;
    int config_version = 30;
    auto status1 = store.load("app", temp1.path(), akasha::FileOptions::create_if_missing);
    auto status2 = store.load("config", temp2.path(), akasha::FileOptions::create_if_missing);
    ASSERT_EQ(status1, akasha::Status::ok);
    ASSERT_EQ(status2, akasha::Status::ok);
    auto set_status1 = store.set<int>("app.version", app_version);
    auto set_status2 = store.set<int>("config.version", config_version);
    ASSERT_EQ(set_status1, akasha::Status::ok);
    ASSERT_EQ(set_status2, akasha::Status::ok);
    ASSERT_TRUE(store.has("app.version"));
    ASSERT_TRUE(store.has("config.version"));
    auto retrieved_app = store.get<int>("app.version");
    ASSERT_EQ(retrieved_app.value(), app_version);
    auto retrieved_config = store.get<int>("config.version");
    ASSERT_EQ(retrieved_config.value(), config_version);
}
