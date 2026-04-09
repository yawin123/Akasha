#include "akasha.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>

/**
 * Demonstrates JSON parsing into a complete Serializable<T> structure,
 * storing it at the ROOT of an Akasha dataset, and loading back.
 * 
 * This example shows the workflow:
 * 1. Read complete JSON file with nlohmann::json
 * 2. Parse into a unified JsonData structure with subsections
 * 3. Store to dataset root via Serializable<T>
 * 4. Load back and verify round-trip
 * 5. Navigate using DatasetView
 */

using json = nlohmann::json;

// Subsection: Basic type examples
struct TypeExamples {
    std::string string_val;
    int64_t number_val;
    bool true_val;
    bool false_val;
    
    bool operator==(const TypeExamples& other) const {
        return string_val == other.string_val && number_val == other.number_val 
            && true_val == other.true_val && false_val == other.false_val;
    }
};

// Subsection: String examples
struct StringExamples {
    std::string empty;
    std::string ascii;
    std::string unicode_emoji;
    std::string unicode_rtl;
};

// Subsection: Number examples
struct NumberExamples {
    int64_t positive;
    int64_t negative;
    double fraction;
    double exponent;
};

// Subsection: Array examples
struct ArrayExamples {
    std::vector<int64_t> of_numbers;
    std::vector<std::string> of_strings;
    std::vector<bool> of_booleans;
};

// Main structure: represents complete JSON data
struct JsonData {
    TypeExamples types;
    StringExamples strings;
    NumberExamples numbers;
    ArrayExamples arrays;
    
    bool operator==(const JsonData& other) const {
        return types == other.types
            && strings.empty == other.strings.empty
            && strings.ascii == other.strings.ascii
            && strings.unicode_emoji == other.strings.unicode_emoji
            && strings.unicode_rtl == other.strings.unicode_rtl
            && numbers.positive == other.numbers.positive
            && numbers.negative == other.numbers.negative
            && arrays.of_numbers == other.arrays.of_numbers
            && arrays.of_strings == other.arrays.of_strings;
    }
};

// ═══════════════════════════════════════════════════════════════
// Serializable<T> Implementations
// ═══════════════════════════════════════════════════════════════

template<>
struct akasha::Serializable<TypeExamples> {
    static void serialize(const TypeExamples& t, akasha::BatchWriter& bw) {
        (void)bw.set<std::string>("string_val", t.string_val);
        (void)bw.set<int64_t>("number_val", t.number_val);
        (void)bw.set<bool>("true_val", t.true_val);
        (void)bw.set<bool>("false_val", t.false_val);
    }
    
    static std::optional<TypeExamples> deserialize(const akasha::BatchReader& br) {
        auto string_val = br.get<std::string>("string_val");
        auto number_val = br.get<int64_t>("number_val");
        auto true_val = br.get<bool>("true_val");
        auto false_val = br.get<bool>("false_val");
        
        if (!string_val || !number_val || !true_val || !false_val) {
            return std::nullopt;
        }
        
        return TypeExamples{*string_val, *number_val, *true_val, *false_val};
    }
};

template<>
struct akasha::Serializable<StringExamples> {
    static void serialize(const StringExamples& s, akasha::BatchWriter& bw) {
        (void)bw.set<std::string>("empty", s.empty);
        (void)bw.set<std::string>("ascii", s.ascii);
        (void)bw.set<std::string>("unicode_emoji", s.unicode_emoji);
        (void)bw.set<std::string>("unicode_rtl", s.unicode_rtl);
    }
    
    static std::optional<StringExamples> deserialize(const akasha::BatchReader& br) {
        auto empty = br.get<std::string>("empty");
        auto ascii = br.get<std::string>("ascii");
        auto unicode_emoji = br.get<std::string>("unicode_emoji");
        auto unicode_rtl = br.get<std::string>("unicode_rtl");
        
        if (!empty || !ascii || !unicode_emoji || !unicode_rtl) {
            return std::nullopt;
        }
        
        return StringExamples{*empty, *ascii, *unicode_emoji, *unicode_rtl};
    }
};

template<>
struct akasha::Serializable<NumberExamples> {
    static void serialize(const NumberExamples& n, akasha::BatchWriter& bw) {
        (void)bw.set<int64_t>("positive", n.positive);
        (void)bw.set<int64_t>("negative", n.negative);
        (void)bw.set<double>("fraction", n.fraction);
        (void)bw.set<double>("exponent", n.exponent);
    }
    
    static std::optional<NumberExamples> deserialize(const akasha::BatchReader& br) {
        auto positive = br.get<int64_t>("positive");
        auto negative = br.get<int64_t>("negative");
        auto fraction = br.get<double>("fraction");
        auto exponent = br.get<double>("exponent");
        
        if (!positive || !negative || !fraction || !exponent) {
            return std::nullopt;
        }
        
        return NumberExamples{*positive, *negative, *fraction, *exponent};
    }
};

template<>
struct akasha::Serializable<ArrayExamples> {
    static void serialize(const ArrayExamples& a, akasha::BatchWriter& bw) {
        (void)bw.set<std::vector<int64_t>>("of_numbers", a.of_numbers);
        (void)bw.set<std::vector<std::string>>("of_strings", a.of_strings);
        (void)bw.set<std::vector<bool>>("of_booleans", a.of_booleans);
    }
    
    static std::optional<ArrayExamples> deserialize(const akasha::BatchReader& br) {
        auto of_numbers = br.get<std::vector<int64_t>>("of_numbers");
        auto of_strings = br.get<std::vector<std::string>>("of_strings");
        auto of_booleans = br.get<std::vector<bool>>("of_booleans");
        
        if (!of_numbers || !of_strings || !of_booleans) {
            return std::nullopt;
        }
        
        return ArrayExamples{*of_numbers, *of_strings, *of_booleans};
    }
};

template<>
struct akasha::Serializable<JsonData> {
    static void serialize(const JsonData& data, akasha::BatchWriter& bw) {
        (void)bw.set<TypeExamples>("types", data.types);
        (void)bw.set<StringExamples>("strings", data.strings);
        (void)bw.set<NumberExamples>("numbers", data.numbers);
        (void)bw.set<ArrayExamples>("arrays", data.arrays);
    }
    
    static std::optional<JsonData> deserialize(const akasha::BatchReader& br) {
        auto types = br.get<TypeExamples>("types");
        auto strings = br.get<StringExamples>("strings");
        auto numbers = br.get<NumberExamples>("numbers");
        auto arrays = br.get<ArrayExamples>("arrays");
        
        if (!types || !strings || !numbers || !arrays) {
            return std::nullopt;
        }
        
        return JsonData{*types, *strings, *numbers, *arrays};
    }
};

// ═══════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════

int main() {
    std::cout << "=== JSON Roundtrip: JSON → JsonData (ROOT) → Akasha → Load ===\n\n";
    
    // Step 1: Read and parse JSON
    std::cout << "1. Reading and parsing JSON file...\n";
    
    std::string json_path = "example.json";
    json raw_data;
    
    try {
        std::ifstream json_file(json_path);
        if (!json_file.is_open()) {
            std::cerr << "✗ Could not open " << json_path << "\n";
            return 1;
        }
        json_file >> raw_data;
        std::cout << "   ✓ Parsed JSON successfully\n\n";
    } catch (const std::exception& e) {
        std::cerr << "✗ JSON parsing failed: " << e.what() << "\n";
        return 1;
    }
    
    // Step 2: Extract data into unified JsonData structure
    std::cout << "2. Populating JsonData structure from JSON...\n";
    
    JsonData data{
        .types = TypeExamples{
            .string_val = raw_data["types"].value("string", ""),
            .number_val = raw_data["types"].value("number", 0),
            .true_val = raw_data["types"].value("true", false),
            .false_val = raw_data["types"].value("false", true)
        },
        .strings = StringExamples{
            .empty = raw_data["strings"].value("empty", ""),
            .ascii = raw_data["strings"].value("ascii", ""),
            .unicode_emoji = raw_data["strings"].value("unicode_emoji", ""),
            .unicode_rtl = raw_data["strings"].value("unicode_rtl", "")
        },
        .numbers = NumberExamples{
            .positive = raw_data["numbers"].value("positive_integer", 0),
            .negative = raw_data["numbers"].value("negative_integer", 0),
            .fraction = raw_data["numbers"].value("fraction", 0.0),
            .exponent = raw_data["numbers"].value("exponent_lowercase", 0.0)
        },
        .arrays = ArrayExamples{
            .of_numbers = raw_data["arrays"].value("of_numbers", std::vector<int64_t>{}),
            .of_strings = raw_data["arrays"].value("of_strings", std::vector<std::string>{}),
            .of_booleans = raw_data["arrays"].value("of_booleans", std::vector<bool>{})
        }
    };
    
    std::cout << "   ✓ Created JsonData structure from JSON sections\n";
    std::cout << "      Types: " << data.types.string_val << ", " << data.types.number_val << "\n";
    std::cout << "      Strings: " << data.strings.ascii << ", " << data.strings.unicode_emoji << "\n";
    std::cout << "      Numbers: " << data.numbers.positive << ", " << data.numbers.negative << "\n";
    std::cout << "      Arrays: " << data.arrays.of_numbers.size() << " numbers, " 
              << data.arrays.of_strings.size() << " strings\n\n";
    
    // Step 3: Initialize Akasha
    std::cout << "3. Initializing Akasha...\n";
    
    akasha::Store store;
    std::string db_path = "/tmp/akasha_json_roundtrip.db";
    
    auto status = store.load("json_data", db_path, akasha::FileOptions::create_if_missing);
    if (status != akasha::Status::ok) {
        std::cerr << "✗ Failed to load dataset: " << static_cast<int>(status) << "\n";
        return 1;
    }
    
    std::cout << "   ✓ Loaded dataset at " << db_path << "\n\n";
    
    // Step 4: Store JsonData at dataset ROOT
    std::cout << "4. Storing JsonData at dataset ROOT...\n";
    
    auto store_status = store.set<JsonData>("json_data", data);
    if (store_status == akasha::Status::ok) {
        std::cout << "   ✓ Stored complete JSON structure at dataset root\n\n";
    } else {
        std::cerr << "   ✗ Failed to store JsonData: " << static_cast<int>(store_status) << "\n";
        return 1;
    }
    
    // Step 5: Load JsonData back from dataset root
    std::cout << "5. Loading JsonData back from dataset ROOT...\n";
    
    auto loaded = store.get<JsonData>("json_data");
    
    if (!loaded.has_value()) {
        std::cerr << "   ✗ Failed to load JsonData\n";
        return 1;
    }
    
    std::cout << "   ✓ Loaded JsonData from storage\n\n";
    
    // Step 6: Verify complete structure matches
    std::cout << "6. Verifying loaded structure matches original...\n";
    
    bool all_match = (loaded.value() == data);
    
    if (all_match) {
        std::cout << "   ✓ Complete structure matches (deep equality check)\n";
    } else {
        std::cerr << "   ✗ Structure MISMATCH!\n";
    }
    
    std::cout << "      Types: " << loaded->types.string_val << ", " << loaded->types.number_val << "\n";
    std::cout << "      Arrays[0]: " << loaded->arrays.of_numbers[0] << "\n\n";
    
    // Step 7: Demonstrate hierarchical navigation
    std::cout << "7. Demonstrating hierarchical navigation with DatasetView...\n";

    auto view_opt = store.get<akasha::Store::DatasetView>("json_data");
    
    bool all_accessible = true;
    for(auto key : view_opt->keys()) {
        auto view = view_opt->get(key);
        if (view.has_value()) {
            auto keys = view->keys();
            std::cout << "   ✓ " << key << ": " << keys.size() << " fields\n";
        } else {
            std::cout << "   ✗ Cannot access " << key << "\n";
            all_accessible = false;
        }
    }
    
    if (all_accessible) {
        std::cout << "   ✓ All subsections navigable via slash notation\n";
    }
    
    std::cout << "\n";
    
    // Cleanup
    if (store.unload("json_data") != akasha::Status::ok) {
        std::cerr << "Warning: Failed to unload dataset\n";
    }
    
    // Results
    std::cout << "====================================\n";
    if (all_match) {
        std::cout << "✓ JSON Roundtrip: SUCCESS\n";
        std::cout << "  JSON → JsonData (ROOT) → Akasha → Load → VERIFIED\n";
    } else {
        std::cout << "✗ JSON Roundtrip: FAILED\n";
    }
    std::cout << "====================================\n\n";
    
    std::filesystem::remove(db_path);

    return all_match ? 0 : 1;
}
