#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: Scalar Types (int64_t, double, bool)
// ============================================================================

TEST(scalars_set_get_int64) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("config", temp.path(), akasha::FileOptions::create_if_missing);
    std::int64_t original = 42;
    auto set_status = store.set<std::int64_t>("config.timeout", original);
    ASSERT_EQ(set_status, akasha::Status::ok);
    ASSERT_TRUE(store.has("config.timeout"));
    auto retrieved = store.get<std::int64_t>("config.timeout");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved.value(), original);
}

TEST(scalars_set_get_double) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("config", temp.path(), akasha::FileOptions::create_if_missing);
    double original = 3.14159;
    auto set_status = store.set<double>("config.pi", original);
    ASSERT_EQ(set_status, akasha::Status::ok);
    ASSERT_TRUE(store.has("config.pi"));
    auto retrieved = store.get<double>("config.pi");
    ASSERT_TRUE(retrieved.has_value());
    // Approximate comparison for floating-point values
    ASSERT_NEAR(retrieved.value(), original, 0.00001);
}

TEST(scalars_set_get_bool) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("config", temp.path(), akasha::FileOptions::create_if_missing);
    bool original = true;
    auto set_status = store.set<bool>("config.enabled", original);
    ASSERT_EQ(set_status, akasha::Status::ok);
    ASSERT_TRUE(store.has("config.enabled"));
    auto retrieved = store.get<bool>("config.enabled");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved.value(), original);
}
