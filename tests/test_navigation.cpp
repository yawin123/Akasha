#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: Navigation (has, keys, DatasetView)
// ============================================================================

TEST(navigation_has_existing_key) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("nav", temp.path(), akasha::FileOptions::create_if_missing);
    auto status = store.set<int>("nav.key1", 123);
    ASSERT_EQ(status, akasha::Status::ok);
    ASSERT_TRUE(store.has("nav.key1"));
}

TEST(navigation_has_nonexistent_key) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("nav", temp.path(), akasha::FileOptions::create_if_missing);
    ASSERT_FALSE(store.has("nav.nonexistent"));
}

TEST(navigation_keys_returns_immediate_children) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("nav", temp.path(), akasha::FileOptions::create_if_missing);
    (void)store.set<int>("nav.a", 1);
    (void)store.set<int>("nav.b", 2);
    (void)store.set<int>("nav.c.nested", 3);
    auto keys = store.get<akasha::Store::DatasetView>("nav").value().keys();
    ASSERT_SIZE(keys, 3);
}

TEST(navigation_datasetview_navigation) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("app", temp.path(), akasha::FileOptions::create_if_missing);
    (void)store.set<int>("app.settings.timeout", 30);
    (void)store.set<int>("app.settings.retries", 5);
    auto view = store.get<akasha::Store::DatasetView>("app.settings");
    ASSERT_TRUE(view.has_value());
    ASSERT_TRUE(view.value().has("timeout"));
}
