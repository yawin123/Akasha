#include "test_framework.hpp"
#include "test_common.hpp"

// ============================================================================
// Tests: Edge Cases Avanzados
// ============================================================================
// Casos límite que requieren validación en operaciones reales

TEST(edge_cases_string_reallocation) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // Write small string
    ASSERT_EQ(store.set<std::string>("db/key", "small"), akasha::Status::ok);
    auto small = store.get<std::string>("db/key");
    ASSERT_TRUE(small.has_value());
    ASSERT_EQ(small.value(), "small");
    
    // Overwrite with much larger string
    std::string large = "this is a much much larger string with significantly more data than before";
    ASSERT_EQ(store.set<std::string>("db/key", large), akasha::Status::ok);
    auto retrieved = store.get<std::string>("db/key");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved.value(), large);
    
    // Contract back to single character
    ASSERT_EQ(store.set<std::string>("db/key", "x"), akasha::Status::ok);
    auto tiny = store.get<std::string>("db/key");
    ASSERT_TRUE(tiny.has_value());
    ASSERT_EQ(tiny.value(), "x");
    
    // Verify no phantom data from previous allocations
    ASSERT_NE(store.get<std::string>("db/key").value().size(), large.size());
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}

TEST(edge_cases_numeric_extremes) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // int64_t limits
    ASSERT_EQ(store.set<int64_t>("db/min", INT64_MIN), akasha::Status::ok);
    ASSERT_EQ(store.set<int64_t>("db/max", INT64_MAX), akasha::Status::ok);
    ASSERT_EQ(store.set<int64_t>("db/zero", 0), akasha::Status::ok);
    ASSERT_EQ(store.set<int64_t>("db/neg_one", -1), akasha::Status::ok);
    
    ASSERT_EQ(store.get<int64_t>("db/min").value(), INT64_MIN);
    ASSERT_EQ(store.get<int64_t>("db/max").value(), INT64_MAX);
    ASSERT_EQ(store.get<int64_t>("db/zero").value(), 0);
    ASSERT_EQ(store.get<int64_t>("db/neg_one").value(), -1);
    
    // double limits
    ASSERT_EQ(store.set<double>("db/near_zero", 1e-308), akasha::Status::ok);
    ASSERT_EQ(store.set<double>("db/large", 1e308), akasha::Status::ok);
    
    auto nz = store.get<double>("db/near_zero").value();
    auto lg = store.get<double>("db/large").value();
    ASSERT_TRUE(nz > 0 && nz < 1e-300);
    ASSERT_TRUE(lg > 1e300);
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}

TEST(edge_cases_large_values) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // Large string (1 MB)
    std::string large_str(1024 * 1024, 'x');
    ASSERT_EQ(store.set<std::string>("db/large_string", large_str), akasha::Status::ok);
    
    auto retrieved_str = store.get<std::string>("db/large_string");
    ASSERT_TRUE(retrieved_str.has_value());
    ASSERT_EQ(retrieved_str->size(), large_str.size());
    ASSERT_EQ(retrieved_str.value(), large_str);
    
    // Large array (100K elements)
    std::vector<int64_t> large_array;
    for (int i = 0; i < 100000; i++) {
        large_array.push_back(i);
    }
    ASSERT_EQ(store.set<std::vector<int64_t>>("db/large_array", large_array), akasha::Status::ok);
    
    auto retrieved_arr = store.get<std::vector<int64_t>>("db/large_array");
    ASSERT_TRUE(retrieved_arr.has_value());
    ASSERT_EQ(retrieved_arr->size(), large_array.size());
    ASSERT_EQ(retrieved_arr.value(), large_array);
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}

TEST(edge_cases_special_key_characters) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // Keys with spaces
    ASSERT_EQ(store.set<int64_t>("db/key with spaces", 1), akasha::Status::ok);
    ASSERT_EQ(store.get<int64_t>("db/key with spaces").value(), 1);
    
    // Keys with quotes
    ASSERT_EQ(store.set<int64_t>("db/key\"with\"quotes", 2), akasha::Status::ok);
    ASSERT_EQ(store.get<int64_t>("db/key\"with\"quotes").value(), 2);
    
    // Keys with newlines (if parser handles them)
    ASSERT_EQ(store.set<int64_t>("db/key\nwith\nnewlines", 3), akasha::Status::ok);
    ASSERT_EQ(store.get<int64_t>("db/key\nwith\nnewlines").value(), 3);
    
    // Keys with multiple consecutive slashes (path-like but valid keys)
    ASSERT_EQ(store.set<int64_t>("db/a/b/c", 4), akasha::Status::ok);
    ASSERT_EQ(store.get<int64_t>("db/a/b/c").value(), 4);
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}

TEST(edge_cases_fragmentation) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // Write many values
    for (int i = 0; i < 1000; i++) {
        std::string key = std::string("db/key_") + std::to_string(i);
        ASSERT_EQ(store.set<std::string>(key, "value"), akasha::Status::ok);
    }
    
    // Clear all (delete pattern)
    for (int i = 0; i < 1000; i++) {
        std::string key = std::string("db/key_") + std::to_string(i);
        ASSERT_EQ(store.clear(key), akasha::Status::ok);
    }
    
    // Write new data — should reuse some space
    for (int i = 0; i < 500; i++) {
        std::string key = std::string("db/new_") + std::to_string(i);
        ASSERT_EQ(store.set<std::string>(key, "new_value"), akasha::Status::ok);
    }
    
    // Verify new data is accessible
    for (int i = 0; i < 500; i++) {
        std::string key = std::string("db/new_") + std::to_string(i);
        auto val = store.get<std::string>(key);
        ASSERT_TRUE(val.has_value());
        ASSERT_EQ(val.value(), "new_value");
    }
    
    // Verify old data is gone
    for (int i = 0; i < 1000; i++) {
        std::string key = std::string("db/key_") + std::to_string(i);
        auto val = store.get<std::string>(key);
        ASSERT_FALSE(val.has_value());
    }
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}

TEST(edge_cases_ambiguous_paths) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // Write scalar at "data"
    ASSERT_EQ(store.set<int64_t>("db/data", 42), akasha::Status::ok);
    
    // Write nested field at "data/field"
    ASSERT_EQ(store.set<int64_t>("db/data/field", 100), akasha::Status::ok);
    
    // Retrieve scalar — should still be accessible
    auto scalar = store.get<int64_t>("db/data");
    ASSERT_TRUE(scalar.has_value());
    ASSERT_EQ(scalar.value(), 42);
    
    // Retrieve nested value
    auto nested = store.get<int64_t>("db/data/field");
    ASSERT_TRUE(nested.has_value());
    ASSERT_EQ(nested.value(), 100);
    
    // Get DatasetView of "db/data" — treats it as hierarchical node
    auto view_opt = store.get<akasha::Store::DatasetView>("db/data");
    ASSERT_TRUE(view_opt.has_value());
    
    // View can access relative paths
    auto field_from_view = view_opt->get<int64_t>("field");
    ASSERT_TRUE(field_from_view.has_value());
    ASSERT_EQ(field_from_view.value(), 100);
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}

TEST(edge_cases_unicode_comprehensive) {
    TempFile temp;
    akasha::Store store;
    ASSERT_EQ(store.load("db", temp.path(), akasha::FileOptions::create_if_missing), 
        akasha::Status::ok);
    
    // RTL text (Arabic)
    std::string rtl = "العربية";
    ASSERT_EQ(store.set<std::string>("db/rtl", rtl), akasha::Status::ok);
    
    // CJK characters
    std::string cjk = "中文 日本語 한국어";
    ASSERT_EQ(store.set<std::string>("db/cjk", cjk), akasha::Status::ok);
    
    // Emoji
    std::string emoji = "🎉🚀🌟";
    ASSERT_EQ(store.set<std::string>("db/emoji", emoji), akasha::Status::ok);
    
    // Combining characters
    std::string combining = "é";  // e + combining accent
    ASSERT_EQ(store.set<std::string>("db/combining", combining), akasha::Status::ok);
    
    // Verify exact preservation
    ASSERT_EQ(store.get<std::string>("db/rtl").value(), rtl);
    ASSERT_EQ(store.get<std::string>("db/rtl")->size(), rtl.size());
    
    ASSERT_EQ(store.get<std::string>("db/cjk").value(), cjk);
    ASSERT_EQ(store.get<std::string>("db/cjk")->size(), cjk.size());
    
    ASSERT_EQ(store.get<std::string>("db/emoji").value(), emoji);
    ASSERT_EQ(store.get<std::string>("db/emoji")->size(), emoji.size());
    
    ASSERT_EQ(store.get<std::string>("db/combining").value(), combining);
    ASSERT_EQ(store.get<std::string>("db/combining")->size(), combining.size());
    
    ASSERT_EQ(store.unload("db"), akasha::Status::ok);
}
