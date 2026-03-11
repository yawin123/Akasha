#include <chrono>
#include <cstdint>
#include <atomic>
#include <iostream>
#include <functional>
#include <random>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>

#include "akasha.hpp"

#define TEST_TIME std::chrono::seconds(1)

// Contar claves de forma recursiva
// ADVERTENCIA: Con agnóstico de tipos, contar muchas claves es caro (O(n) por calls a keys())
// Para evitar rendimiento catastrófico, limitamos a prefijos pequeños (max 10k claves)
size_t countKeysRecursive(const akasha::Store::DatasetView& view)
{
    size_t total = 0;
    const auto keys_list = view.keys();
    
    // Si hay demasiadas claves en este nivel, contar solo el nivel actual
    // (evita O(n²) cuando hay 500k+ claves)
    constexpr size_t RECURSIVE_LIMIT = 10000;
    if (keys_list.size() > RECURSIVE_LIMIT) {
        return keys_list.size();
    }

    for (const auto& key : keys_list) {
        const auto sub_view = view.get<akasha::Store::DatasetView>(key);
        if (!sub_view.has_value()) {
            ++total;
            continue;
        }

        const auto sub_keys = sub_view->keys();
        if (sub_keys.empty()) {
            // Es una hoja
            ++total;
        } else {
            // Es un nodo, explorar recursivamente
            total += countKeysRecursive(*sub_view);
        }
    }

    return total;
}

// Test 1: Almacenar cadena de valores (cada valor es el anterior)
// Mantiene la misma lógica: valor = anterior, durante TEST_TIME
size_t test1(akasha::Store& store)
{
    std::uint64_t initial_value = 123456;
    std::uint64_t counter = 0;
    const auto end_time = std::chrono::steady_clock::now() + TEST_TIME;

    while (std::chrono::steady_clock::now() < end_time) {
        ++counter;

        const std::string actual_key = "test.1." + std::to_string(counter);
        const std::string last_key = "test.1." + std::to_string(counter - 1);

        std::uint64_t value_to_store = initial_value;
        if (counter > 1) {
            const auto last_value = store.get<std::uint64_t>(last_key);
            if (!last_value.has_value()) {
                std::cerr << "get failed for last_key: " << last_key << '\n';
                return 0;
            }
            value_to_store = *last_value;
        }

        const auto set_status = store.set<std::uint64_t>(actual_key, value_to_store);
        if (set_status != akasha::Status::ok) {
            std::cerr << "set failed with status: " << static_cast<int>(set_status) << '\n';
            return 0;
        }
    }

    // Contar todas las claves de forma recursiva (como hacía el original)
    const auto view = store.get("test.1");
    if (!view.has_value()) {
        return 0;
    }
    return countKeysRecursive(*view);
}

// Test 1b: Igual que test1 pero con N iteraciones fijas, retorna tiempo en microsegundos
std::uint64_t test1_iterations(akasha::Store& store, std::uint64_t iterations)
{
    std::uint64_t initial_value = 123456;
    
    const auto start = std::chrono::steady_clock::now();

    for (std::uint64_t counter = 1; counter <= iterations; ++counter) {
        const std::string actual_key = "test.1b." + std::to_string(counter);
        const std::string last_key = "test.1b." + std::to_string(counter - 1);

        std::uint64_t value_to_store = initial_value;
        if (counter > 1) {
            const auto last_value = store.get<std::uint64_t>(last_key);
            if (!last_value.has_value()) {
                std::cerr << "get failed for last_key: " << last_key << '\n';
                return 0;
            }
            value_to_store = *last_value;
        }

        const auto set_status = store.set<std::uint64_t>(actual_key, value_to_store);
        if (set_status != akasha::Status::ok) {
            std::cerr << "set failed with status: " << static_cast<int>(set_status) << '\n';
            return 0;
        }
    }

    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

// Fill test: Almacena N claves con valores aleatorios (sin relación entre ellos)
void fill_random(akasha::Store& store, const std::string& key_prefix, std::uint64_t count)
{
    std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<std::uint64_t> dist;

    for (std::uint64_t i = 1; i <= count; ++i) {
        const std::string key = key_prefix + std::to_string(i);
        const std::uint64_t random_value = dist(rng);
        const auto set_status = store.set<std::uint64_t>(key, random_value);
        if (set_status != akasha::Status::ok) {
            std::cerr << "set failed with status: " << static_cast<int>(set_status) << '\n';
            return;
        }
    }
}

void print_storage_content(const akasha::Store& store, const std::string& key_prefix, std::uint64_t count)
{
    for (std::uint64_t i = 1; i <= count; ++i) {
        const std::string key = key_prefix + std::to_string(i);
        const auto value = store.get<std::uint64_t>(key);
        if (value.has_value()) {
            std::cout << key << " = " << *value << '\n';
        } else {
            std::cout << key << " not found\n";
        }
    }
}

template <typename Func, typename... Args>
auto duration_wrapper(Func&& func, Args&&... args)
{
    const auto start = std::chrono::steady_clock::now();
    auto result = std::invoke(func, std::forward<Args>(args)...);
    const auto end = std::chrono::steady_clock::now();
    const auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    return duration_us;
}

// Read test
std::uint64_t reads_test(akasha::Store& store, const std::string& key_prefix, std::uint64_t iterations)
{
    std::uint64_t success_count = 0;
    for(std::uint64_t i = 1; i <= iterations; ++i) {
        const std::string key = key_prefix + std::to_string(i);
        const auto value = store.get<std::uint64_t>(key);
        if(value.has_value()) {
            ++success_count;
        }
    }
    return success_count;
}

// Read test (random keys)
std::uint64_t reads_test_random(akasha::Store& store, const std::string& key_prefix, std::uint64_t iterations)
{
    std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<std::uint64_t> dist(1, iterations);

    std::uint64_t success_count = 0;
    for(std::uint64_t i = 1; i <= iterations; ++i) {
        const std::string key = key_prefix + std::to_string(dist(rng));
        const auto value = store.get<std::uint64_t>(key);
        if(value.has_value()) {
            ++success_count;
        }
    }
    return success_count;
}

// Write test
std::uint64_t writes_test(akasha::Store& store, const std::string& key_prefix, std::uint64_t iterations)
{
    std::uint64_t success_count = 0;
    for(std::uint64_t i = 1; i <= iterations; ++i) {
        const std::string key = key_prefix + std::to_string(i);
        const auto set_status = store.set<std::uint64_t>(key, i);
        if(set_status == akasha::Status::ok) {
            ++success_count;
        }
    }
    return success_count;
}

// Custom struct para demostrar tipos personalizados
struct Point3D {
    float x, y, z;
};

// Almacenar un dato de cada tipo
void store_all_types(akasha::Store& store)
{
    std::cout << "=== Almacenando datos de diferentes tipos ===\n";
    
    // uint64_t
    const auto status_u64 = store.set<std::uint64_t>("test.types.uint64", 0xDEADBEEFCAFEBABEULL);
    std::cout << "uint64_t: " << (status_u64 == akasha::Status::ok ? "OK" : "FAIL") << '\n';
    
    // uint32_t
    const auto status_u32 = store.set<std::uint32_t>("test.types.uint32", 0xDEADBEEFU);
    std::cout << "uint32_t: " << (status_u32 == akasha::Status::ok ? "OK" : "FAIL") << '\n';
    
    // int32_t
    const auto status_i32 = store.set<std::int32_t>("test.types.int32", -42);
    std::cout << "int32_t: " << (status_i32 == akasha::Status::ok ? "OK" : "FAIL") << '\n';
    
    // double
    const auto status_double = store.set<double>("test.types.double", 3.141592653589793);
    std::cout << "double: " << (status_double == akasha::Status::ok ? "OK" : "FAIL") << '\n';
    
    // float
    const auto status_float = store.set<float>("test.types.float", 2.71828f);
    std::cout << "float: " << (status_float == akasha::Status::ok ? "OK" : "FAIL") << '\n';
    
    // bool
    const auto status_bool_true = store.set<bool>("test.types.bool_true", true);
    std::cout << "bool (true): " << (status_bool_true == akasha::Status::ok ? "OK" : "FAIL") << '\n';
    
    const auto status_bool_false = store.set<bool>("test.types.bool_false", false);
    std::cout << "bool (false): " << (status_bool_false == akasha::Status::ok ? "OK" : "FAIL") << '\n';
    
    // char
    const auto status_char = store.set<char>("test.types.char", 'A');
    std::cout << "char: " << (status_char == akasha::Status::ok ? "OK" : "FAIL") << '\n';
    
    // struct personalizado
    const auto status_struct = store.set<Point3D>("test.types.point3d", {1.5f, 2.5f, 3.5f});
    std::cout << "Point3D struct: " << (status_struct == akasha::Status::ok ? "OK" : "FAIL") << '\n';
    
    // std::string
    const auto status_string = store.set<std::string>("test.types.string", "Hola, Akasha!");
    std::cout << "std::string: " << (status_string == akasha::Status::ok ? "OK" : "FAIL") << '\n';
    
    // Empty string
    const auto status_empty_string = store.set<std::string>("test.types.empty_string", "");
    std::cout << "std::string (empty): " << (status_empty_string == akasha::Status::ok ? "OK" : "FAIL") << '\n';    
    std::cout << "\n=== Recuperando datos almacenados ===\n";
    
    if (const auto val = store.get<std::uint64_t>("test.types.uint64")) {
        std::cout << "uint64: 0x" << std::hex << *val << std::dec << '\n';
    }
    if (const auto val = store.get<std::uint32_t>("test.types.uint32")) {
        std::cout << "uint32: 0x" << std::hex << *val << std::dec << '\n';
    }
    if (const auto val = store.get<std::int32_t>("test.types.int32")) {
        std::cout << "int32: " << *val << '\n';
    }
    if (const auto val = store.get<double>("test.types.double")) {
        std::cout << "double: " << *val << '\n';
    }
    if (const auto val = store.get<float>("test.types.float")) {
        std::cout << "float: " << *val << '\n';
    }
    if (const auto val = store.get<bool>("test.types.bool_true")) {
        std::cout << "bool_true: " << (*val ? "true" : "false") << '\n';
    }
    if (const auto val = store.get<bool>("test.types.bool_false")) {
        std::cout << "bool_false: " << (*val ? "true" : "false") << '\n';
    }
    if (const auto val = store.get<char>("test.types.char")) {
        std::cout << "char: " << *val << '\n';
    }
    if (const auto val = store.get<Point3D>("test.types.point3d")) {
        std::cout << "Point3D: (" << val->x << ", " << val->y << ", " << val->z << ")\n";
    }
    if (const auto val = store.get<std::string>("test.types.string")) {
        std::cout << "string: \"" << *val << "\"\n";
    }
    if (const auto val = store.get<std::string>("test.types.empty_string")) {
        std::cout << "empty_string: \"" << *val << "\" (length: " << val->size() << ")\n";
    }    std::cout << '\n';
}

// Demostración de getorset: obtener o establecer con valor por defecto
void demo_getorset(akasha::Store& store)
{
    std::cout << "=== Demo getorset (obtener o establecer default) ===\n";
    
    // Primera llamada: no existe, se establece el default
    const auto val1 = store.getorset<std::uint64_t>("test.getorset.counter", 100ULL);
    std::cout << "Intento 1 - counter (no existe): " << (val1.has_value() ? std::to_string(*val1) : "FAIL") << '\n';
    
    // Segunda llamada: ya existe, retorna el valor anterior
    const auto val2 = store.getorset<std::uint64_t>("test.getorset.counter", 200ULL);
    std::cout << "Intento 2 - counter (existe): " << (val2.has_value() ? std::to_string(*val2) : "FAIL") << '\n';
    
    // String: primera vez establece "config-default"
    const auto str1 = store.getorset<std::string>("test.getorset.config", "config-default");
    std::cout << "String intento 1 (no existe): " << (str1.has_value() ? "\"" + *str1 + "\"" : "FAIL") << '\n';
    
    // String: segunda vez retorna el valor anterior
    const auto str2 = store.getorset<std::string>("test.getorset.config", "otro-default");
    std::cout << "String intento 2 (existe): " << (str2.has_value() ? "\"" + *str2 + "\"" : "FAIL") << '\n';
    
    // Double con default
    const auto dbl = store.getorset<double>("test.getorset.threshold", 0.95);
    std::cout << "Double (no existe): " << (dbl.has_value() ? std::to_string(*dbl) : "FAIL") << '\n';
    
    std::cout << '\n';
}

int main(int argc, char* argv[]) {
    std::uint64_t iterations = 100000;

    //Obtenemos iterations de argumentos si se proporcionan
    if (argc > 1) {
        try {
            iterations = std::stoull(argv[1]);
        } catch (const std::exception& e) {
            std::cerr << "Invalid iterations argument: " << argv[1] << ". Using default: " << iterations << '\n';
        }
    }

    akasha::Store store;

    akasha::PerformanceTuning tuning;
    tuning.initial_mapped_file_size = 64 * 1024;
    tuning.initial_grow_step = tuning.initial_mapped_file_size / 2;
    tuning.max_grow_retries = 8;
    store.set_performance_tuning(tuning);

    std::string file_path = "build/test_loop.mmap";

    const auto load_status = store.load("test", file_path, true);
    if (load_status != akasha::Status::ok) {
        std::cerr << "load failed with status: " << static_cast<int>(load_status) << '\n';
        return 1;
    }

    store_all_types(store);

    demo_getorset(store);

    fill_random(store, "test.", iterations);
    std::cout << "Read test (" << iterations << " iterations, consecutive access): " << duration_wrapper(reads_test, store, "test.", iterations) << " µs\n";
    std::cout << "Read test (" << iterations << " iterations, random access): " << duration_wrapper(reads_test_random, store, "test.", iterations) << " µs\n";
    if(store.clear() != akasha::Status::ok)  std::cerr << "Failed to clear the store.\n";

    std::cout << "Write test (" << iterations << " iterations, cleared store): " << duration_wrapper(writes_test, store, "test.", iterations) << " µs\n";
    std::cout << "Write test (" << iterations << " iterations, filled store): " << duration_wrapper(writes_test, store, "test.", iterations) << " µs\n";

    if(store.clear() != akasha::Status::ok) {
        std::cerr << "Failed to clear the store.\n";
    }

    std::filesystem::remove(file_path);  // Limpiar archivo previo para pruebas consistentes

    return 0;
}

