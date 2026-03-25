#include "test_framework.hpp"
#include "test_common.hpp"
#include <limits>

// ============================================================================
// Tests: Type Mismatches and Large Values
// ============================================================================

TEST(typeandsize_type_mismatch_int_as_double) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    // Store as int
    (void)store.set<int>("data.value", 42);
    // Retrieve as double - undefined behavior but must not crash
    auto retrieved = store.get<double>("data.value");
}

TEST(typeandsize_type_mismatch_string_as_int) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    // Store as string
    (void)store.set<std::string>("data.text", "hello");
    // Retrieve as int - undefined behavior but must not crash
    auto retrieved = store.get<int>("data.text");
}

TEST(typeandsize_type_correct_type_retrieval) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    (void)store.set<int>("data.int_val", 42);
    (void)store.set<double>("data.double_val", 3.14);
    (void)store.set<std::string>("data.string_val", "test");
    (void)store.set<bool>("data.bool_val", true);
    // Retrieve with correct types
    ASSERT_EQ(store.get<int>("data.int_val").value(), 42);
    ASSERT_NEAR(store.get<double>("data.double_val").value(), 3.14, 0.01);
    ASSERT_EQ(store.get<std::string>("data.string_val").value(), "test");
    ASSERT_EQ(store.get<bool>("data.bool_val").value(), true);
}

TEST(typeandsize_type_large_int64_values) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    std::int64_t min_val = std::numeric_limits<std::int64_t>::min();
    std::int64_t max_val = std::numeric_limits<std::int64_t>::max();
    (void)store.set<std::int64_t>("data.min", min_val);
    (void)store.set<std::int64_t>("data.max", max_val);
    ASSERT_EQ(store.get<std::int64_t>("data.min").value(), min_val);
    ASSERT_EQ(store.get<std::int64_t>("data.max").value(), max_val);
}

TEST(typeandsize_type_file_growth_during_writes) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    auto initial_size = fs::file_size(temp.path());
    // Write enough data to cause observable growth
    for (int i = 0; i < 50; ++i) {
        std::string key = "data.key_" + std::to_string(i);
        std::string value = "value_" + std::string(100, 'x');
        (void)store.set<std::string>(key, value);
    }
    auto final_size = fs::file_size(temp.path());
    // File should grow or stay same, not shrink
    ASSERT_GE(final_size, initial_size);
}
