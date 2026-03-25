// ============================================================================
// Simple Test Framework
// ============================================================================
//
// Author: Yawin
// License: GNU LGPLv3
// Repository: https://git.yawin.es/personal/simple-test-framework
//
// A lightweight, header-only test framework for C++ applications.
// Provides basic test registration, assertions, and reporting.
//
// ============================================================================

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <cstring>
#include <cmath>

namespace stf {

/** @brief Simple test result tracking */
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

/** @brief Global test registry */
static std::vector<TestResult> g_results;

/** @brief Register a test result */
inline void register_result(const std::string& name, bool passed, const std::string& message = "") {
    g_results.push_back({name, passed, message});
}

/** @brief Print test summary */
inline void print_summary() {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Test Summary\n";
    std::cout << std::string(60, '=') << "\n";
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& result : g_results) {
        if (result.passed) {
            std::cout << "✓ " << result.name << "\n";
            passed++;
        } else {
            std::cout << "✗ " << result.name;
            if (!result.message.empty()) {
                std::cout << " - " << result.message;
            }
            std::cout << "\n";
            failed++;
        }
    }
    
    std::cout << std::string(60, '=') << "\n";
    std::cout << "Passed: " << passed << " | Failed: " << failed << " | Total: " << (passed + failed) << "\n";
    std::cout << std::string(60, '=') << "\n";
}

/** @brief Return exit code based on test results */
inline int exit_code() {
    for (const auto& result : g_results) {
        if (!result.passed) return 1;
    }
    return 0;
}

}  // namespace stf

// ============================================================================
// Assertion Macros
// ============================================================================

/** @brief Assert equality */
#define ASSERT_EQ(actual, expected) \
    { \
        if ((actual) != (expected)) { \
            std::ostringstream oss; \
            oss << "ASSERT_EQ failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert inequality */
#define ASSERT_NE(actual, expected) \
    { \
        if ((actual) == (expected)) { \
            std::ostringstream oss; \
            oss << "ASSERT_NE failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert true */
#define ASSERT_TRUE(condition) \
    { \
        if (!(condition)) { \
            std::ostringstream oss; \
            oss << "ASSERT_TRUE failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert false */
#define ASSERT_FALSE(condition) \
    { \
        if ((condition)) { \
            std::ostringstream oss; \
            oss << "ASSERT_FALSE failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

// ============================================================================
// Comparison Assertions (Grupo 1)
// ============================================================================

/** @brief Assert greater than */
#define ASSERT_GT(actual, expected) \
    { \
        if (!((actual) > (expected))) { \
            std::ostringstream oss; \
            oss << "ASSERT_GT failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert less than */
#define ASSERT_LT(actual, expected) \
    { \
        if (!((actual) < (expected))) { \
            std::ostringstream oss; \
            oss << "ASSERT_LT failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert greater than or equal */
#define ASSERT_GE(actual, expected) \
    { \
        if (!((actual) >= (expected))) { \
            std::ostringstream oss; \
            oss << "ASSERT_GE failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert less than or equal */
#define ASSERT_LE(actual, expected) \
    { \
        if (!((actual) <= (expected))) { \
            std::ostringstream oss; \
            oss << "ASSERT_LE failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

// ============================================================================
// Floating Point Assertions (Grupo 3)
// ============================================================================

/** @brief Assert near (with tolerance) */
#define ASSERT_NEAR(actual, expected, tolerance) \
    { \
        auto diff = std::abs((actual) - (expected)); \
        if (!(diff <= (tolerance))) { \
            std::ostringstream oss; \
            oss << "ASSERT_NEAR failed at " << __FILE__ << ":" << __LINE__ \
                << " (difference: " << diff << ", tolerance: " << tolerance << ")"; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert double equality (with epsilon 1e-6) */
#define ASSERT_DOUBLE_EQ(actual, expected) \
    { \
        auto diff = std::abs((static_cast<double>(actual)) - (static_cast<double>(expected))); \
        if (!(diff <= 1e-6)) { \
            std::ostringstream oss; \
            oss << "ASSERT_DOUBLE_EQ failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert float equality (with epsilon 1e-5) */
#define ASSERT_FLOAT_EQ(actual, expected) \
    { \
        auto diff = std::abs((static_cast<float>(actual)) - (static_cast<float>(expected))); \
        if (!(diff <= 1e-5f)) { \
            std::ostringstream oss; \
            oss << "ASSERT_FLOAT_EQ failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

// ============================================================================
// Pointer Assertions (Grupo 5)
// ============================================================================

/** @brief Assert null pointer */
#define ASSERT_NULL(ptr) \
    { \
        if ((ptr) != nullptr) { \
            std::ostringstream oss; \
            oss << "ASSERT_NULL failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert not null pointer */
#define ASSERT_NOT_NULL(ptr) \
    { \
        if ((ptr) == nullptr) { \
            std::ostringstream oss; \
            oss << "ASSERT_NOT_NULL failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

// ============================================================================
// String Assertions (Grupo 6)
// ============================================================================

/** @brief Assert string equality */
#define ASSERT_STREQ(actual, expected) \
    { \
        std::string_view a = (actual); \
        std::string_view e = (expected); \
        if (a != e) { \
            std::ostringstream oss; \
            oss << "ASSERT_STREQ failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert string inequality */
#define ASSERT_STRNE(actual, expected) \
    { \
        std::string_view a = (actual); \
        std::string_view e = (expected); \
        if (a == e) { \
            std::ostringstream oss; \
            oss << "ASSERT_STRNE failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert string contains substring */
#define ASSERT_CONTAINS(str, substr) \
    { \
        std::string_view _stf_str = (str); \
        std::string_view _stf_sub = (substr); \
        if (_stf_str.find(_stf_sub) == std::string_view::npos) { \
            std::ostringstream oss; \
            oss << "ASSERT_CONTAINS failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert string starts with prefix */
#define ASSERT_STARTS_WITH(str, prefix) \
    { \
        std::string_view _stf_str = (str); \
        std::string_view _stf_pre = (prefix); \
        if (!_stf_str.starts_with(_stf_pre)) { \
            std::ostringstream oss; \
            oss << "ASSERT_STARTS_WITH failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert string ends with suffix */
#define ASSERT_ENDS_WITH(str, suffix) \
    { \
        std::string_view _stf_str = (str); \
        std::string_view _stf_suf = (suffix); \
        if (!_stf_str.ends_with(_stf_suf)) { \
            std::ostringstream oss; \
            oss << "ASSERT_ENDS_WITH failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

// ============================================================================
// Container Assertions (Grupo 7)
// ============================================================================

/** @brief Assert container is empty */
#define ASSERT_EMPTY(container) \
    { \
        if (!(container).empty()) { \
            std::ostringstream oss; \
            oss << "ASSERT_EMPTY failed at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    }

/** @brief Assert container size */
#define ASSERT_SIZE(container, expected_size) \
    { \
        if ((container).size() != (expected_size)) { \
            std::ostringstream oss; \
            oss << "ASSERT_SIZE failed at " << __FILE__ << ":" << __LINE__ \
                << " (expected: " << (expected_size) << ", actual: " << (container).size() << ")"; \
            throw std::runtime_error(oss.str()); \
        } \
    }

// ============================================================================
// Test Registration Macro
// ============================================================================

/** @brief Define and register a test function
 *  Usage: TEST(my_test_name) { ASSERT_EQ(1, 1); }
 */
#define TEST(test_name) \
    void test_##test_name(); \
    namespace { \
        struct TestRegistrar_##test_name { \
            TestRegistrar_##test_name() { \
                try { \
                    test_##test_name(); \
                    stf::register_result(#test_name, true); \
                } catch (const std::exception& e) { \
                    stf::register_result(#test_name, false, e.what()); \
                } catch (...) { \
                    stf::register_result(#test_name, false, "Unknown exception"); \
                } \
            } \
        } g_registrar_##test_name; \
    } \
    void test_##test_name()
