#include "test_framework.hpp"

// All tests are organized in separate files by theme:
#include "test_load_unload.cpp"     //Load/unload and persistence tests
#include "test_scalars.cpp"         //Scalar types (int64_t, double, bool)
#include "test_strings.cpp"         //String storage and retrieval
#include "test_navigation.cpp"      //Navigation (has, keys, DatasetView)
#include "test_errors.cpp"          //Error handling and status codes
#include "test_performance.cpp"     //Performance tuning
#include "test_overwrite.cpp"       //Overwriting values with different sizes
#include "test_deep_paths.cpp"      //Deep nested paths and intermediate levels
#include "test_root_values.cpp"     //Dataset root values
#include "test_invalid_paths.cpp"   //Invalid path format validation
#include "test_load_errors.cpp"     //Load/unload errors
#include "test_type_and_size.cpp"   //Type mismatches and large values
#include "test_datasetview.cpp"     //Complex nested DatasetView navigation
#include "test_vectors.cpp"         //Vector serialization and deserialization
#include "test_structs.cpp"         //Struct storage: same type, mixed types, many members

int main() {
    std::cout << "Running Akasha Test Suite...\n";
    stf::print_summary();
    return stf::exit_code();
}
