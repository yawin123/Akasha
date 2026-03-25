// Test suite for trivially-copyable struct storage
//
// Tests structures with:
// 1. Multiple members of the same type
// 2. Multiple members of different types
// 3. Many many members

#include "test_framework.hpp"
#include "test_common.hpp"
#include <cstdint>

// ============================================================================
// Test 1: Structure with multiple members of the same type
// ============================================================================

struct AllIntsStruct {
    int a;
    int b;
    int c;
    int d;
    int e;
};

TEST(structs_same_type_basic) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    
    AllIntsStruct original{42, 100, -5, 999, 12345};
    ASSERT_EQ(store.set<AllIntsStruct>("data.config", original), akasha::Status::ok);
    
    auto retrieved = store.get<AllIntsStruct>("data.config");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->a, 42);
    ASSERT_EQ(retrieved->b, 100);
    ASSERT_EQ(retrieved->c, -5);
    ASSERT_EQ(retrieved->d, 999);
    ASSERT_EQ(retrieved->e, 12345);
}

TEST(structs_same_type_overwrite) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    
    // First write
    AllIntsStruct first{10, 20, 30, 40, 50};
    ASSERT_EQ(store.set<AllIntsStruct>("data.nums", first), akasha::Status::ok);
    auto retrieved1 = store.get<AllIntsStruct>("data.nums");
    ASSERT_EQ(retrieved1->a, 10);
    
    // Overwrite with different values
    AllIntsStruct second{-1, -2, -3, -4, -5};
    ASSERT_EQ(store.set<AllIntsStruct>("data.nums", second), akasha::Status::ok);
    auto retrieved2 = store.get<AllIntsStruct>("data.nums");
    ASSERT_EQ(retrieved2->a, -1);
    ASSERT_EQ(retrieved2->b, -2);
    ASSERT_EQ(retrieved2->c, -3);
    ASSERT_EQ(retrieved2->d, -4);
    ASSERT_EQ(retrieved2->e, -5);
}

// ============================================================================
// Test 2: Structure with multiple members of different types
// ============================================================================

struct MixedTypesStruct {
    int8_t byte_val;
    int16_t short_val;
    int32_t int_val;
    int64_t long_val;
    float float_val;
    double double_val;
    bool bool_val;
};

TEST(structs_mixed_types_basic) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    
    MixedTypesStruct original{
        127,           // int8_t
        32000,         // int16_t
        2000000,       // int32_t
        9000000000LL,  // int64_t
        3.14f,         // float
        2.71828,       // double
        true           // bool
    };
    
    ASSERT_EQ(store.set<MixedTypesStruct>("data.mixed", original), akasha::Status::ok);
    
    auto retrieved = store.get<MixedTypesStruct>("data.mixed");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->byte_val, 127);
    ASSERT_EQ(retrieved->short_val, 32000);
    ASSERT_EQ(retrieved->int_val, 2000000);
    ASSERT_EQ(retrieved->long_val, 9000000000LL);
    ASSERT_NEAR(retrieved->float_val, 3.14f, 0.01f);
    ASSERT_NEAR(retrieved->double_val, 2.71828, 0.00001);
    ASSERT_TRUE(retrieved->bool_val);
}

TEST(structs_mixed_types_overwrite) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    
    // First write
    MixedTypesStruct first{1, 2, 3, 4, 1.0f, 2.0, false};
    ASSERT_EQ(store.set<MixedTypesStruct>("data.values", first), akasha::Status::ok);
    
    // Overwrite with different values
    MixedTypesStruct second{-1, -2, -3, -4, 0.5f, 0.25, true};
    ASSERT_EQ(store.set<MixedTypesStruct>("data.values", second), akasha::Status::ok);
    
    auto retrieved = store.get<MixedTypesStruct>("data.values");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->byte_val, -1);
    ASSERT_EQ(retrieved->short_val, -2);
    ASSERT_EQ(retrieved->int_val, -3);
    ASSERT_EQ(retrieved->long_val, -4);
    ASSERT_NEAR(retrieved->float_val, 0.5f, 0.01f);
    ASSERT_NEAR(retrieved->double_val, 0.25, 0.00001);
    ASSERT_TRUE(retrieved->bool_val);
}

// ============================================================================
// Test 3: Structure with many many members
// ============================================================================

struct LargeMembersStruct {
    // 96 members (various types) - doubled from original 48
    uint32_t m0, m1, m2, m3, m4, m5, m6, m7, m8, m9;
    int32_t m10, m11, m12, m13, m14, m15, m16, m17, m18, m19;
    uint16_t m20, m21, m22, m23, m24, m25, m26, m27, m28, m29;
    int16_t m30, m31, m32, m33, m34, m35, m36, m37, m38, m39;
    uint8_t m40, m41, m42, m43, m44, m45, m46, m47, m48, m49;
    int8_t m50, m51, m52, m53, m54, m55, m56, m57, m58, m59;
    float m60, m61, m62, m63, m64, m65, m66, m67, m68, m69;
    double m70, m71, m72, m73, m74, m75, m76, m77, m78, m79;
    bool m80, m81, m82, m83, m84, m85, m86, m87, m88, m89, m90, m91, m92, m93, m94, m95;
};

TEST(structs_many_members_basic) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    
    LargeMembersStruct original{
        // uint32_t: 0-9
        1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
        // int32_t: 10-19
        -1000, -2000, -3000, -4000, -5000, -6000, -7000, -8000, -9000, -10000,
        // uint16_t: 20-29
        100, 200, 300, 400, 500, 600, 700, 800, 900, 1000,
        // int16_t: 30-39
        -100, -200, -300, -400, -500, -600, -700, -800, -900, -1000,
        // uint8_t: 40-49
        10, 20, 30, 40, 50, 60, 70, 80, 90, 100,
        // int8_t: 50-59
        -10, -20, -30, -40, -50, -60, -70, -80, -90, -100,
        // float: 60-69
        1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, 7.7f, 8.8f, 9.9f, 10.1f,
        // double: 70-79
        1.11, 2.22, 3.33, 4.44, 5.55, 6.66, 7.77, 8.88, 9.99, 10.11,
        // bool: 80-95 (16 members)
        true, false, true, false, true, false, true, false, true, false, true, false, true, false, true, false
    };
    
    ASSERT_EQ(store.set<LargeMembersStruct>("data.large", original), akasha::Status::ok);
    
    auto retrieved = store.get<LargeMembersStruct>("data.large");
    ASSERT_TRUE(retrieved.has_value());
    
    // Verify uint32_t members (m0-m9)
    ASSERT_EQ(retrieved->m0, 1000);
    ASSERT_EQ(retrieved->m4, 5000);
    ASSERT_EQ(retrieved->m9, 10000);
    
    // Verify int32_t members (m10-m19)
    ASSERT_EQ(retrieved->m10, -1000);
    ASSERT_EQ(retrieved->m14, -5000);
    ASSERT_EQ(retrieved->m19, -10000);
    
    // Verify uint16_t members (m20-m29)
    ASSERT_EQ(retrieved->m20, 100);
    ASSERT_EQ(retrieved->m24, 500);
    ASSERT_EQ(retrieved->m29, 1000);
    
    // Verify int16_t members (m30-m39)
    ASSERT_EQ(retrieved->m30, -100);
    ASSERT_EQ(retrieved->m34, -500);
    ASSERT_EQ(retrieved->m39, -1000);
    
    // Verify uint8_t members (m40-m49)
    ASSERT_EQ(retrieved->m40, 10);
    ASSERT_EQ(retrieved->m44, 50);
    ASSERT_EQ(retrieved->m49, 100);
    
    // Verify int8_t members (m50-m59)
    ASSERT_EQ(retrieved->m50, -10);
    ASSERT_EQ(retrieved->m54, -50);
    ASSERT_EQ(retrieved->m59, -100);
    
    // Verify float members (m60-m69, with tolerance)
    ASSERT_NEAR(retrieved->m60, 1.1f, 0.01f);
    ASSERT_NEAR(retrieved->m64, 5.5f, 0.01f);
    ASSERT_NEAR(retrieved->m69, 10.1f, 0.01f);
    
    // Verify double members (m70-m79, with tolerance)
    ASSERT_NEAR(retrieved->m70, 1.11, 0.001);
    ASSERT_NEAR(retrieved->m74, 5.55, 0.001);
    ASSERT_NEAR(retrieved->m79, 10.11, 0.001);
    
    // Verify bool members (m80-m95)
    ASSERT_TRUE(retrieved->m80);
    ASSERT_FALSE(retrieved->m81);
    ASSERT_TRUE(retrieved->m90);
    ASSERT_FALSE(retrieved->m95);
}

TEST(structs_many_members_overwrite_size) {
    TempFile temp;
    akasha::Store store;
    (void)store.load("data", temp.path(), akasha::FileOptions::create_if_missing);
    
    // Write first large struct
    LargeMembersStruct s1{
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        -10, -10, -10, -10, -10, -10, -10, -10, -10, -10,
        100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
        -100, -100, -100, -100, -100, -100, -100, -100, -100, -100,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
        true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true
    };
    
    ASSERT_EQ(store.set<LargeMembersStruct>("data.s", s1), akasha::Status::ok);
    
    // Overwrite with completely different values
    LargeMembersStruct s2{
        999, 888, 777, 666, 555, 444, 333, 222, 111, 50000,
        -999, -888, -777, -666, -555, -444, -333, -222, -111, -50000,
        99, 88, 77, 66, 55, 44, 33, 22, 11, 999,
        -99, -88, -77, -66, -55, -44, -33, -22, -11, -999,
        9, 8, 7, 6, 5, 4, 3, 2, 1, 255,
        -9, -8, -7, -6, -5, -4, -3, -2, -1, -128,
        9.9f, 8.8f, 7.7f, 6.6f, 5.5f, 4.4f, 3.3f, 2.2f, 1.1f, 10.5f,
        9.99, 8.88, 7.77, 6.66, 5.55, 4.44, 3.33, 2.22, 1.11, 10.55,
        false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false
    };
    
    ASSERT_EQ(store.set<LargeMembersStruct>("data.s", s2), akasha::Status::ok);
    
    auto retrieved = store.get<LargeMembersStruct>("data.s");
    ASSERT_TRUE(retrieved.has_value());
    // Verify numeric members from s2 (completely different from s1)
    ASSERT_EQ(retrieved->m0, 999);
    ASSERT_EQ(retrieved->m4, 555);
    ASSERT_EQ(retrieved->m9, 50000);
    ASSERT_EQ(retrieved->m10, -999);
    ASSERT_EQ(retrieved->m19, -50000);
    ASSERT_EQ(retrieved->m40, 9);
    ASSERT_EQ(retrieved->m49, 255);
}
