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
    (void)store.set<int64_t>("data/value", 42);
    // Retrieve as double - undefined behavior but must not crash
    auto retrieved = store.get<double>("data/value");
}

TEST(typeandsize_type_mismatch_string_as_int) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    // Store as string
    (void)store.set<std::string>("data/text", "hello");
    // Retrieve as int - undefined behavior but must not crash
    auto retrieved = store.get<int64_t>("data/text");
}

TEST(typeandsize_type_correct_type_retrieval) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    (void)store.set<int64_t>("data/int_val", 42);
    (void)store.set<double>("data/double_val", 3.14);
    (void)store.set<std::string>("data/string_val", "test");
    (void)store.set<bool>("data/bool_val", true);
    // Retrieve with correct types
    ASSERT_EQ(store.get<int64_t>("data/int_val").value(), 42);
    ASSERT_NEAR(store.get<double>("data/double_val").value(), 3.14, 0.01);
    ASSERT_EQ(store.get<std::string>("data/string_val").value(), "test");
    ASSERT_EQ(store.get<bool>("data/bool_val").value(), true);
}

TEST(typeandsize_type_large_int64_values) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    std::int64_t min_val = std::numeric_limits<std::int64_t>::min();
    std::int64_t max_val = std::numeric_limits<std::int64_t>::max();
    (void)store.set<std::int64_t>("data/min", min_val);
    (void)store.set<std::int64_t>("data/max", max_val);
    ASSERT_EQ(store.get<std::int64_t>("data/min").value(), min_val);
    ASSERT_EQ(store.get<std::int64_t>("data/max").value(), max_val);
}

TEST(typeandsize_type_file_growth_during_writes) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    auto initial_size = fs::file_size(temp.path());
    // Write enough data to cause observable growth
    for (int i = 0; i < 50; ++i) {
        std::string key = "data/key_" + std::to_string(i);
        std::string value = "value_" + std::string(100, 'x');
        (void)store.set<std::string>(key, value);
    }
    auto final_size = fs::file_size(temp.path());
    // File should grow or stay same, not shrink
    ASSERT_GE(final_size, initial_size);
}


// ============================================================================
// Tests: Generic numeric types: int, float, short, unsigned, range checks
// ============================================================================

TEST(typeandsize_generic_int_types) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	// int
	ASSERT_EQ(store.set<int>("db/a", 42), akasha::Status::ok);
	auto a = store.get<int>("db/a");
	ASSERT_TRUE(a.has_value());
	ASSERT_EQ(*a, 42);

	// short
	ASSERT_EQ(store.set<short>("db/b", static_cast<short>(-123)), akasha::Status::ok);
	auto b = store.get<short>("db/b");
	ASSERT_TRUE(b.has_value());
	ASSERT_EQ(*b, -123);

	// unsigned int
	ASSERT_EQ(store.set<unsigned>("db/c", 1000u), akasha::Status::ok);
	auto c = store.get<unsigned>("db/c");
	ASSERT_TRUE(c.has_value());
	ASSERT_EQ(*c, 1000u);

	// int64_t (canonical, still works)
	ASSERT_EQ(store.set<int64_t>("db/d", INT64_C(9999999999)), akasha::Status::ok);
	auto d = store.get<int64_t>("db/d");
	ASSERT_TRUE(d.has_value());
	ASSERT_EQ(*d, INT64_C(9999999999));

	// Cross-type: written as int, read as int64_t
	auto a_wide = store.get<int64_t>("db/a");
	ASSERT_TRUE(a_wide.has_value());
	ASSERT_EQ(*a_wide, 42);
}

TEST(typeandsize_generic_float_types) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	// float
	ASSERT_EQ(store.set<float>("db/f", 3.14f), akasha::Status::ok);
	auto f = store.get<float>("db/f");
	ASSERT_TRUE(f.has_value());
	ASSERT_NEAR(*f, 3.14f, 0.001f);

	// double (canonical, still works)
	ASSERT_EQ(store.set<double>("db/d", 2.718281828), akasha::Status::ok);
	auto d = store.get<double>("db/d");
	ASSERT_TRUE(d.has_value());
	ASSERT_NEAR(*d, 2.718281828, 0.0001);

	// Cross-type: stored as float, read as double
	auto f_as_d = store.get<double>("db/f");
	ASSERT_TRUE(f_as_d.has_value());
	ASSERT_NEAR(*f_as_d, 3.14, 0.01);
}

TEST(typeandsize_generic_overflow_and_range) {
	TempFile temp;
	akasha::Store store;
	ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), akasha::Status::ok);

	// uint64_t > INT64_MAX → se almacena y recupera correctamente (bit-cast)
	uint64_t huge = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
	ASSERT_EQ(store.set<uint64_t>("db/huge", huge), akasha::Status::ok);
	auto retrieved_huge = store.get<uint64_t>("db/huge");
	ASSERT_TRUE(retrieved_huge.has_value());
	ASSERT_EQ(*retrieved_huge, huge);

	// Large int64_t → doesn't fit in int32 → nullopt
	ASSERT_EQ(store.set<int64_t>("db/big", INT64_C(5000000000)), akasha::Status::ok);
	auto as_int = store.get<int>("db/big");
	ASSERT_FALSE(as_int.has_value());

	// Same value readable as int64_t
	auto as_i64 = store.get<int64_t>("db/big");
	ASSERT_TRUE(as_i64.has_value());
	ASSERT_EQ(*as_i64, INT64_C(5000000000));

	// Negative stored → read as unsigned → nullopt
	ASSERT_EQ(store.set<int>("db/neg", -1), akasha::Status::ok);
	auto as_unsigned = store.get<unsigned>("db/neg");
	ASSERT_FALSE(as_unsigned.has_value());
}