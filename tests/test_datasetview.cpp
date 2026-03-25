#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: Nested DatasetView Navigation
// ============================================================================

TEST(datasetview_nested_navigation) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("app", temp.path(), akasha::FileOptions::create_if_missing);
    (void)store.set<int>("app.settings.timeout", 30);
    (void)store.set<std::string>("app.settings.host", "localhost");
    // Get view from level 1
    auto app_view = store.get<akasha::Store::DatasetView>("app");
    ASSERT_TRUE(app_view.has_value());
    // Get view from level 2
    auto settings_view = store.get<akasha::Store::DatasetView>("app.settings");
    ASSERT_TRUE(settings_view.has_value());
}

TEST(datasetview_has_method) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("app", temp.path(), akasha::FileOptions::create_if_missing);
    (void)store.set<int>("app.config.db.port", 3306);
    (void)store.set<std::string>("app.config.db.host", "localhost");
    auto config_view = store.get<akasha::Store::DatasetView>("app.config");
    ASSERT_TRUE(config_view.has_value());
    // Verify expected keys exist
    ASSERT_TRUE(config_view.value().has("db.port"));
    ASSERT_TRUE(config_view.value().has("db.host"));
    // Verify nonexistent key is not found
    ASSERT_FALSE(config_view.value().has("db.nonexistent"));
}

TEST(datasetview_keys_method) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("app", temp.path(), akasha::FileOptions::create_if_missing);
    (void)store.set<int>("app.config.db.port", 3306);
    (void)store.set<std::string>("app.config.db.host", "localhost");
    (void)store.set<int>("app.config.cache.ttl", 3600);
    auto config_view = store.get<akasha::Store::DatasetView>("app.config");
    auto keys = config_view.value().keys();
    // Immediate children: db and cache
    ASSERT_SIZE(keys, 2);
}

TEST(datasetview_multilevel_structure_verification) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("app", temp.path(), akasha::FileOptions::create_if_missing);
    // Create multilevel structure
    (void)store.set<int>("app.config.database.port", 5432);
    (void)store.set<std::string>("app.config.database.name", "mydb");
    (void)store.set<std::string>("app.config.cache.redis_host", "localhost");
    (void)store.set<int>("app.config.cache.redis_port", 6379);
    (void)store.set<bool>("app.config.debug.enabled", true);
    // Get view from intermediate level
    auto config_view = store.get<akasha::Store::DatasetView>("app.config");
    ASSERT_TRUE(config_view.has_value());
    // Verify complete structure
    ASSERT_TRUE(config_view.value().has("database.port"));
    ASSERT_TRUE(config_view.value().has("database.name"));
    ASSERT_TRUE(config_view.value().has("cache.redis_host"));
    ASSERT_TRUE(config_view.value().has("cache.redis_port"));
    ASSERT_TRUE(config_view.value().has("debug.enabled"));
    // Verify values from view
    auto db_view = store.get<akasha::Store::DatasetView>("app.config.database");
    ASSERT_TRUE(db_view.has_value());
    ASSERT_TRUE(db_view.value().has("port"));
    ASSERT_TRUE(db_view.value().has("name"));
    // Verify exact values
    ASSERT_EQ(store.get<int>("app.config.database.port").value(), 5432);
    ASSERT_EQ(store.get<std::string>("app.config.database.name").value(), "mydb");
    ASSERT_EQ(store.get<std::string>("app.config.cache.redis_host").value(), "localhost");
    ASSERT_EQ(store.get<int>("app.config.cache.redis_port").value(), 6379);
    ASSERT_EQ(store.get<bool>("app.config.debug.enabled").value(), true);
}

TEST(datasetview_empty_keys) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    // Nothing is inserted
    auto view = store.get<akasha::Store::DatasetView>("data");
    ASSERT_TRUE(view.has_value());
    auto keys = view.value().keys();
    ASSERT_EMPTY(keys);
}

TEST(datasetview_invalid_file_path) {
    akasha::Store store;
    // Empty path is invalid
    auto status = store.load("data", "", akasha::FileOptions::create_if_missing);
    ASSERT_EQ(status, akasha::Status::invalid_file_path);
}

TEST(datasetview_file_read_error) {
    // This test depends on system-specific behavior
    // Simply verify that status is set on error paths
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    // Try to access unloaded dataset
    auto result = store.get<int>("nonexistent.key");
    ASSERT_EQ(store.last_status(), akasha::Status::dataset_not_found);
}
