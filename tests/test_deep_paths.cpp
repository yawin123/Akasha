#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: Deep Paths and Intermediate Levels
// ============================================================================

TEST(deeppaths_deeply_nested_paths) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    // Create path with 4 levels
    auto status = store.set<int>("data.app.config.db.timeout", 5000);
    ASSERT_EQ(status, akasha::Status::ok);
    auto retrieved = store.get<int>("data.app.config.db.timeout");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved.value(), 5000);
}

TEST(deeppaths_access_intermediate_level) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("app", temp.path(), akasha::FileOptions::create_if_missing);
    (void)store.set<int>("app.config.db.port", 3306);
    (void)store.set<std::string>("app.config.db.host", "localhost");
    // Get view from intermediate level
    auto view = store.get<akasha::Store::DatasetView>("app.config");
    ASSERT_TRUE(view.has_value());
    ASSERT_TRUE(view.value().has("db.port"));
    ASSERT_TRUE(view.value().has("db.host"));
}

TEST(deeppaths_keys_at_intermediate_depth) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("app", temp.path(), akasha::FileOptions::create_if_missing);
    (void)store.set<int>("app.config.db.port", 3306);
    (void)store.set<std::string>("app.config.db.host", "localhost");
    (void)store.set<int>("app.config.cache.ttl", 3600);
    auto view = store.get<akasha::Store::DatasetView>("app.config");
    auto keys = view.value().keys();
    // keys() returns only immediate children: db and cache
    ASSERT_SIZE(keys, 2);
}
