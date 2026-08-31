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
    (void)store.set<int64_t>("dataset1/key", 42);
    ASSERT_TRUE(store.has("dataset1/key"));
    ASSERT_EQ(store.get<int64_t>("dataset1/key").value(), 42);
    auto status = store.unload("dataset1");
    ASSERT_EQ(status, akasha::Status::ok);
    ASSERT_FALSE(store.has("dataset1/key"));
}

TEST(loadunload_data_persists_after_unload) {
    TempFile temp;
    int value = 999;
    // First session: write data
    {
        akasha::Store store1;
        auto status = store1.load("data", temp.path(), akasha::FileOptions::create_if_missing);
        ASSERT_EQ(status, akasha::Status::ok);
        auto set_status = store1.set<int64_t>("data/value", value);
        ASSERT_EQ(set_status, akasha::Status::ok);
        auto unload_status = store1.unload("data");
        ASSERT_EQ(unload_status, akasha::Status::ok);
    }
    // Second session: reopen and verify
    {
        akasha::Store store2;
        auto status = store2.load("data", temp.path(), akasha::FileOptions::none);
        ASSERT_EQ(status, akasha::Status::ok);
        auto retrieved = store2.get<int64_t>("data/value");
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
    auto set_status1 = store.set<int64_t>("app/version", app_version);
    auto set_status2 = store.set<int64_t>("config/version", config_version);
    ASSERT_EQ(set_status1, akasha::Status::ok);
    ASSERT_EQ(set_status2, akasha::Status::ok);
    ASSERT_TRUE(store.has("app/version"));
    ASSERT_TRUE(store.has("config/version"));
    auto retrieved_app = store.get<int64_t>("app/version");
    ASSERT_EQ(retrieved_app.value(), app_version);
    auto retrieved_config = store.get<int64_t>("config/version");
    ASSERT_EQ(retrieved_config.value(), config_version);
}

TEST(loadunload_clear_in_memory) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ((store.load("data", temp.path(), akasha::FileOptions::create_if_missing)), 
        akasha::Status::ok);
    
    // Set an unordered_map
    std::unordered_map<std::string, int64_t> user_map;
    user_map["alice"] = 100;
    user_map["bob"] = 200;
    ASSERT_EQ((store.set<std::unordered_map<std::string, int64_t>>("data/users", user_map)), 
        akasha::Status::ok);
    
    // Verify it exists
    ASSERT_TRUE(store.has("data/users"));
    auto users_before = (store.get<std::unordered_map<std::string, int64_t>>("data/users"));
    ASSERT_TRUE(users_before.has_value());
    ASSERT_SIZE(users_before.value(), size_t(2));
    
    // Clear it
    ASSERT_EQ(store.clear("data/users"), akasha::Status::ok);
    
    // Verify it's gone (still in same session, in-memory)
    ASSERT_FALSE(store.has("data/users"));
    
    auto users_after = (store.get<std::unordered_map<std::string, int64_t>>("data/users"));
    ASSERT_FALSE(users_after.has_value());
}

TEST(loadunload_clear_persists_after_unload_reload) {
    TempFile temp;
    // ─ First session: write multiple data types and then clear one ──────────
    {
        akasha::Store store;
        ASSERT_EQ((store.load("data", temp.path(), akasha::FileOptions::create_if_missing)), 
            akasha::Status::ok);
        
        // Set a scalar value
        ASSERT_EQ((store.set<int64_t>("data/config/port", 8080)), akasha::Status::ok);
        ASSERT_EQ((store.set<std::string>("data/config/host", "localhost")), akasha::Status::ok);
        
        // Set an unordered_map
        std::unordered_map<std::string, int64_t> user_map;
        user_map["alice"] = 100;
        user_map["bob"] = 200;
        user_map["charlie"] = 300;
        ASSERT_EQ((store.set<std::unordered_map<std::string, int64_t>>("data/users", user_map)), 
            akasha::Status::ok);
        
        // Set another scalar value
        ASSERT_EQ((store.set<std::string>("data/version", "1.0.0")), akasha::Status::ok);
        
        // Verify all data exists before clearing
        ASSERT_TRUE(store.has("data/config/port"));
        ASSERT_TRUE(store.has("data/config/host"));
        ASSERT_TRUE(store.has("data/users"));
        ASSERT_TRUE(store.has("data/version"));
        
        auto users_before = (store.get<std::unordered_map<std::string, int64_t>>("data/users"));
        ASSERT_TRUE(users_before.has_value());
        ASSERT_SIZE(users_before.value(), size_t(3));
        
        // Also set some nested scalar values under data/config
        ASSERT_EQ(store.clear("data/config/host"), akasha::Status::ok);
        
        // Clear the unordered_map (delete "data/users" and all its children)
        ASSERT_EQ(store.clear("data/users"), akasha::Status::ok);
        
        // Verify cleared data is gone but other data persists in this session
        ASSERT_FALSE(store.has("data/users"));
        ASSERT_FALSE(store.has("data/config/host"));
        ASSERT_TRUE(store.has("data/config/port"));
        ASSERT_TRUE(store.has("data/version"));
        
        // Unload to persist changes
        ASSERT_EQ(store.unload("data"), akasha::Status::ok);
    }
    
    // ─ Second session: reload and verify the deletion persisted ────────────
    {
        akasha::Store store;
        ASSERT_EQ((store.load("data", temp.path(), akasha::FileOptions::none)), 
            akasha::Status::ok);
        
        // Verify cleared keys are still gone after reload
        ASSERT_FALSE(store.has("data/users"));
        ASSERT_FALSE(store.has("data/config/host"));
        
        auto users_after = (store.get<std::unordered_map<std::string, int64_t>>("data/users"));
        ASSERT_FALSE(users_after.has_value());
        
        auto host_after = (store.get<std::string>("data/config/host"));
        ASSERT_FALSE(host_after.has_value());
        
        // Verify surviving data is still there
        ASSERT_TRUE(store.has("data/config/port"));
        ASSERT_TRUE(store.has("data/version"));
        
        auto port = (store.get<int64_t>("data/config/port"));
        ASSERT_TRUE(port.has_value());
        ASSERT_EQ(port.value(), 8080);
        
        auto version = (store.get<std::string>("data/version"));
        ASSERT_TRUE(version.has_value());
        ASSERT_EQ(version.value(), "1.0.0");
    }
}
