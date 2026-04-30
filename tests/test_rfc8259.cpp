#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: RFC 8259 Complete Coverage
// ============================================================================
// This test file covers full RFC 8259 JSON data model support:
// - Scalars: bool, int64_t, double, strings, null
// - Arrays: empty, homogeneous, of objects
// - Objects: empty, with all value types, special keys
// - Nesting: objects in arrays, arrays in fields, deep nesting (5+ levels)

// ── Test Structures ──────────────────────────────────────────────────────

struct SimpleScalar {
    int64_t integer;
    double floating;
    bool flag;
    std::string text;
    
    bool operator==(const SimpleScalar& other) const {
        return integer == other.integer && 
               floating == other.floating &&
               flag == other.flag &&
               text == other.text;
    }
};

template<>
struct akasha::Serializable<SimpleScalar> {
    static void serialize(const SimpleScalar& s, akasha::BatchWriter& bw) {
        (void)bw.set("integer", s.integer);
        (void)bw.set("floating", s.floating);
        (void)bw.set("flag", s.flag);
        (void)bw.set("text", s.text);
    }
    
    static std::optional<SimpleScalar> deserialize(const akasha::BatchReader& br) {
        auto integer = br.get<int64_t>("integer");
        auto floating = br.get<double>("floating");
        auto flag = br.get<bool>("flag");
        auto text = br.get<std::string>("text");
        
        if (!integer || !floating || !flag || !text) return std::nullopt;
        return SimpleScalar{*integer, *floating, *flag, *text};
    }
};

// ── Tests ────────────────────────────────────────────────────────────────

TEST(rfc8259_scalars_all_types) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // Scalars: int64, double, bool, string, null
    ASSERT_EQ(store.set<int64_t>("db/int", 42), akasha::Status::ok);
    ASSERT_EQ(store.set<double>("db/float", 3.14159), akasha::Status::ok);
    ASSERT_EQ(store.set<bool>("db/true", true), akasha::Status::ok);
    ASSERT_EQ(store.set<bool>("db/false", false), akasha::Status::ok);
    ASSERT_EQ(store.set<std::string>("db/text", "hello world"), akasha::Status::ok);
    ASSERT_EQ(store.set_null("db/null"), akasha::Status::ok);
    
    // Verify retrieval
    ASSERT_EQ(store.get<int64_t>("db/int").value(), 42);
    ASSERT_TRUE(std::abs(store.get<double>("db/float").value() - 3.14159) < 1e-5);
    ASSERT_EQ(store.get<bool>("db/true").value(), true);
    ASSERT_EQ(store.get<bool>("db/false").value(), false);
    ASSERT_EQ(store.get<std::string>("db/text").value(), "hello world");
    ASSERT_FALSE(store.get<int64_t>("db/null").has_value());  // null → no value
    ASSERT_TRUE(store.has("db/null"));  // but exists
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}

TEST(rfc8259_strings_comprehensive) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // String edge cases
    ASSERT_EQ(store.set<std::string>("db/empty", ""), akasha::Status::ok);
    ASSERT_EQ(store.set<std::string>("db/spaces", "  whitespace  "), akasha::Status::ok);
    ASSERT_EQ(store.set<std::string>("db/escaped", "line1\nline2\ttab"), akasha::Status::ok);
    ASSERT_EQ(store.set<std::string>("db/unicode", "日本語 🚀"), akasha::Status::ok);
    
    // Verify
    ASSERT_EQ(store.get<std::string>("db/empty").value(), "");
    ASSERT_EQ(store.get<std::string>("db/spaces").value(), "  whitespace  ");
    ASSERT_EQ(store.get<std::string>("db/escaped").value(), "line1\nline2\ttab");
    ASSERT_EQ(store.get<std::string>("db/unicode").value(), "日本語 🚀");
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}

TEST(rfc8259_arrays_empty_and_scalar) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // Empty arrays
    std::vector<int64_t> empty_int;
    ASSERT_EQ(store.set<std::vector<int64_t>>("db/empty_int", empty_int), akasha::Status::ok);
    
    std::vector<std::string> empty_str;
    ASSERT_EQ(store.set<std::vector<std::string>>("db/empty_str", empty_str), akasha::Status::ok);
    
    // Scalar arrays
    std::vector<int64_t> ints = {1, 2, 3, 4, 5};
    ASSERT_EQ(store.set<std::vector<int64_t>>("db/ints", ints), akasha::Status::ok);
    
    std::vector<double> floats = {1.1, 2.2, 3.3};
    ASSERT_EQ(store.set<std::vector<double>>("db/floats", floats), akasha::Status::ok);
    
    std::vector<bool> bools = {true, false, true};
    ASSERT_EQ(store.set<std::vector<bool>>("db/bools", bools), akasha::Status::ok);
    
    std::vector<std::string> strings = {"a", "b", "c"};
    ASSERT_EQ(store.set<std::vector<std::string>>("db/strings", strings), akasha::Status::ok);
    
    // Verify
    ASSERT_EQ(store.get<std::vector<int64_t>>("db/empty_int").value().size(), 0);
    ASSERT_EQ(store.get<std::vector<std::string>>("db/empty_str").value().size(), 0);
    ASSERT_EQ(store.get<std::vector<int64_t>>("db/ints").value(), ints);
    ASSERT_EQ(store.get<std::vector<double>>("db/floats").value(), floats);
    ASSERT_EQ(store.get<std::vector<bool>>("db/bools").value(), bools);
    ASSERT_EQ(store.get<std::vector<std::string>>("db/strings").value(), strings);
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}

TEST(rfc8259_serializable_types) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // Custom types
    SimpleScalar s1{42, 3.14, true, "hello"};
    ASSERT_EQ(store.set<SimpleScalar>("db/scalar", s1), akasha::Status::ok);
    
    auto s2 = store.get<SimpleScalar>("db/scalar");
    ASSERT_TRUE(s2.has_value());
    ASSERT_EQ(s2.value(), s1);
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}

TEST(rfc8259_object_with_all_types) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // Set wide variety of value types
    ASSERT_EQ(store.set<int64_t>("db/obj/int_field", 10), akasha::Status::ok);
    ASSERT_EQ(store.set<double>("db/obj/float_field", 2.71), akasha::Status::ok);
    ASSERT_EQ(store.set<bool>("db/obj/bool_field", true), akasha::Status::ok);
    ASSERT_EQ(store.set<std::string>("db/obj/str_field", "text"), akasha::Status::ok);
    ASSERT_EQ(store.set_null("db/obj/null_field"), akasha::Status::ok);
    
    // Empty collections
    std::vector<int64_t> empty;
    ASSERT_EQ(store.set<std::vector<int64_t>>("db/obj/empty_array", empty), akasha::Status::ok);
    
    // Verify through DatasetView
    auto obj_opt = store.get<akasha::Store::DatasetView>("db/obj");
    ASSERT_TRUE(obj_opt.has_value());
    auto& obj = obj_opt.value();
    
    auto int_val = obj.get<int64_t>("int_field");
    ASSERT_TRUE(int_val.has_value());
    ASSERT_EQ(int_val.value(), 10);
    
    auto float_val = obj.get<double>("float_field");
    ASSERT_TRUE(float_val.has_value());
    ASSERT_TRUE(std::abs(float_val.value() - 2.71) < 1e-2);
    
    auto bool_val = obj.get<bool>("bool_field");
    ASSERT_TRUE(bool_val.has_value());
    ASSERT_EQ(bool_val.value(), true);
    
    auto str_val = obj.get<std::string>("str_field");
    ASSERT_TRUE(str_val.has_value());
    ASSERT_EQ(str_val.value(), "text");
    
    auto null_val = obj.get<int64_t>("null_field");
    ASSERT_FALSE(null_val.has_value());
    ASSERT_TRUE(obj.has("null_field"));
    
    auto arr_val = obj.get<std::vector<int64_t>>("empty_array");
    ASSERT_TRUE(arr_val.has_value());
    ASSERT_EQ(arr_val.value().size(), 0);
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}

TEST(rfc8259_deep_nesting_objects) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // 5-level nesting: db/l1/l2/l3/l4/l5
    // Set through Store using the full path (works because Store::set creates intermediate paths)
    ASSERT_EQ(store.set<std::string>("db/l1/l2/l3/l4/l5/leaf", "deep_value"), akasha::Status::ok);
    
    // Verify by retrieving from the deepest path
    auto leaf_val = store.get<std::string>("db/l1/l2/l3/l4/l5/leaf");
    ASSERT_TRUE(leaf_val.has_value());
    ASSERT_EQ(leaf_val.value(), "deep_value");
    
    // Also verify we can get DatasetView of intermediate levels once they exist
    auto l1_opt = store.get<akasha::Store::DatasetView>("db/l1");
    ASSERT_TRUE(l1_opt.has_value());
    
    auto l3_opt = store.get<akasha::Store::DatasetView>("db/l1/l2/l3");
    ASSERT_TRUE(l3_opt.has_value());
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}

TEST(rfc8259_roundtrip_complex) {
    TempFile temp;
    
    // Session 1: Write
    {
        akasha::Store store;
        ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
            akasha::Status::ok);
        
        ASSERT_EQ(store.set<int64_t>("db/num", 999), akasha::Status::ok);
        ASSERT_EQ(store.set<std::string>("db/text", "roundtrip"), akasha::Status::ok);
        
        std::vector<int64_t> vec = {10, 20, 30};
        ASSERT_EQ(store.set<std::vector<int64_t>>("db/arr", vec), akasha::Status::ok);
        
        ASSERT_EQ(store.set<bool>("db/obj/active", true), akasha::Status::ok);
        ASSERT_EQ(store.set_null("db/obj/empty"), akasha::Status::ok);
        
        ASSERT_EQ(store.unload("db"), akasha::Status::ok);
    }
    
    // Session 2: Read and verify
    {
        akasha::Store store;
        ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::none), 
            akasha::Status::ok);
        
        auto num_val = store.get<int64_t>("db/num");
        ASSERT_TRUE(num_val.has_value());
        ASSERT_EQ(num_val.value(), 999);
        
        auto text_val = store.get<std::string>("db/text");
        ASSERT_TRUE(text_val.has_value());
        ASSERT_EQ(text_val.value(), "roundtrip");
        
        auto vec_val = store.get<std::vector<int64_t>>("db/arr");
        ASSERT_TRUE(vec_val.has_value());
        ASSERT_EQ(vec_val.value().size(), 3);
        ASSERT_EQ(vec_val.value()[0], 10);
        
        auto obj_opt = store.get<akasha::Store::DatasetView>("db/obj");
        ASSERT_TRUE(obj_opt.has_value());
        
        auto active = obj_opt->get<bool>("active");
        ASSERT_TRUE(active.has_value());
        ASSERT_EQ(active.value(), true);
        
        auto empty = obj_opt->get<int64_t>("empty");
        ASSERT_FALSE(empty.has_value());
        
        ASSERT_EQ(store.unload("db"), akasha::Status::ok);
    }
    
    // Session 3: Write again and verify same structure
    {
        akasha::Store store;
        ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::none), 
            akasha::Status::ok);
        
        // Overwrite
        ASSERT_EQ(store.set<int64_t>("db/num", 999), akasha::Status::ok);
        
        ASSERT_EQ(store.unload("db"), akasha::Status::ok);
    }
    
    // Session 4: Final verification
    {
        akasha::Store store;
        ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::none), 
            akasha::Status::ok);
        
        auto num_val = store.get<int64_t>("db/num");
        ASSERT_TRUE(num_val.has_value());
        ASSERT_EQ(num_val.value(), 999);
        
        ASSERT_EQ(store.unload("db"), akasha::Status::ok);
    }
}
