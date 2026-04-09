// ============================================================================
// Akasha Benchmarks - Main
// ============================================================================

#include "benchmark.hpp"
#include "common.hpp"
#include "akasha.hpp"
#include <fstream>

// ============================================================================
// Context structs (using raw pointers, cleaned up in teardown)
// ============================================================================
struct ScalarCtx {
    TempFile temp = TempFile(".db");
    akasha::Store store;
};

struct VectorCtx {
    TempFile temp = TempFile(".db");
    akasha::Store store;
    std::vector<double> vec;
};

struct SceneCtx {
    TempFile temp = TempFile(".db");
    akasha::Store store;
    Scene scene;
};

// ============================================================================
// Benchmark 1: Load dataset (empty)
// ============================================================================
void load_empty_it_setup(sbf::Benchmark& bm) {
    auto ctx = new ScalarCtx();
    bm.set_local_context(ctx);
}
void load_empty_run(sbf::Benchmark& bm) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    (void)ctx->store.load("bench", ctx->temp.path(), akasha::FileOptions::create_if_missing);
    BENCHMARK_OPERATION;
}
void load_empty_it_teardown(sbf::Benchmark& bm) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    (void)ctx->store.unload("bench");
    delete ctx;
}

BENCHMARK_REGISTER_FULL(load_empty_dataset, nullptr, load_empty_it_setup, load_empty_run, load_empty_it_teardown, nullptr);

// ============================================================================
// Benchmark 2: Sequential scalar writes
// ============================================================================
void write_scalar_keys_it_setup(sbf::Benchmark& bm) {
    auto ctx = new ScalarCtx();
    (void)ctx->store.load("bench", ctx->temp.path(), akasha::FileOptions::create_if_missing);
    bm.set_local_context(ctx);
}
void write_scalar_keys_run(sbf::Benchmark& bm, int nkeys) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    for (size_t i = 0; i < nkeys; ++i) {
        (void)ctx->store.set<int64_t>("bench/value/" + std::to_string(i), static_cast<int64_t>(i));
        BENCHMARK_OPERATION;
    }
}
void write_scalar_keys_run_1000(sbf::Benchmark& bm) {
    write_scalar_keys_run(bm, 1000);
}
void write_scalar_keys_run_10000(sbf::Benchmark& bm) {
    write_scalar_keys_run(bm, 10000);
}
void write_scalar_keys_run_100000(sbf::Benchmark& bm) {
    write_scalar_keys_run(bm, 100000);
}
void write_scalar_keys_it_teardown(sbf::Benchmark& bm) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    (void)ctx->store.unload("bench");
    delete ctx;
}

BENCHMARK_REGISTER_FULL(write_scalar_keys, nullptr, write_scalar_keys_it_setup, write_scalar_keys_run_1000, write_scalar_keys_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(write_scalar_keys_10k, nullptr, write_scalar_keys_it_setup, write_scalar_keys_run_10000, write_scalar_keys_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(write_scalar_keys_100k, nullptr, write_scalar_keys_it_setup, write_scalar_keys_run_100000, write_scalar_keys_it_teardown, nullptr);

// ============================================================================
// Benchmark 3: Sequential scalar reads
// ============================================================================
void read_scalar_keys_it_setup(sbf::Benchmark& bm, int nkeys) {
    auto ctx = new ScalarCtx();
    (void)ctx->store.load("bench", ctx->temp.path(), akasha::FileOptions::create_if_missing);

    // Pre-populate
    for (size_t i = 0; i < nkeys; ++i) {
        (void)ctx->store.set<int64_t>("bench/value/" + std::to_string(i), static_cast<int64_t>(i));
    }

    bm.set_local_context(ctx);
}
void read_scalar_keys_it_setup_1000(sbf::Benchmark& bm) {
    read_scalar_keys_it_setup(bm, 1000);
}
void read_scalar_keys_it_setup_10000(sbf::Benchmark& bm) {
    read_scalar_keys_it_setup(bm, 10000);
}
void read_scalar_keys_it_setup_100000(sbf::Benchmark& bm) {
    read_scalar_keys_it_setup(bm, 100000);
}
void read_scalar_keys_run(sbf::Benchmark& bm, int nkeys) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    for (size_t i = 0; i < nkeys; ++i) {
        auto val = ctx->store.get<int64_t>("bench/value/" + std::to_string(rand() % nkeys));
        BENCHMARK_OPERATION;
        (void)val;
    }
}
void read_scalar_keys_run_1000(sbf::Benchmark& bm) {
    read_scalar_keys_run(bm, 1000);
}
void read_scalar_keys_run_10000(sbf::Benchmark& bm) {
    read_scalar_keys_run(bm, 10000);
}
void read_scalar_keys_run_100000(sbf::Benchmark& bm) {
    read_scalar_keys_run(bm, 100000);
}
void read_scalar_keys_it_teardown(sbf::Benchmark& bm) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    (void)ctx->store.unload("bench");
    delete ctx;
}

BENCHMARK_REGISTER_FULL(read_scalar_keys_1k, nullptr, read_scalar_keys_it_setup_1000, read_scalar_keys_run_1000, read_scalar_keys_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(read_scalar_keys_10k, nullptr, read_scalar_keys_it_setup_10000, read_scalar_keys_run_10000, read_scalar_keys_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(read_scalar_keys_100k, nullptr, read_scalar_keys_it_setup_100000, read_scalar_keys_run_100000, read_scalar_keys_it_teardown, nullptr);

// ============================================================================
// Benchmark 4: Sequential string writes
// ============================================================================
void write_string_keys_it_setup(sbf::Benchmark& bm) {
    auto ctx = new ScalarCtx();
    (void)ctx->store.load("bench", ctx->temp.path(), akasha::FileOptions::create_if_missing);
    bm.set_local_context(ctx);
}
void write_string_keys_run(sbf::Benchmark& bm, int nkeys) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    for (size_t i = 0; i < nkeys; ++i) {
        std::string value = "Value number " + std::to_string(i) + " with some extra padding";
        (void)ctx->store.set<std::string>("bench/str/" + std::to_string(i), value);
        BENCHMARK_OPERATION;
    }
}
void write_string_keys_run_1000(sbf::Benchmark& bm) {
    write_string_keys_run(bm, 1000);
}
void write_string_keys_run_10000(sbf::Benchmark& bm) {
    write_string_keys_run(bm, 10000);
}
void write_string_keys_run_100000(sbf::Benchmark& bm) {
    write_string_keys_run(bm, 100000);
}
void write_string_keys_it_teardown(sbf::Benchmark& bm) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    (void)ctx->store.unload("bench");
    delete ctx;
}

BENCHMARK_REGISTER_FULL(write_string_keys_1k, nullptr, write_string_keys_it_setup, write_string_keys_run_1000, write_string_keys_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(write_string_keys_10k, nullptr, write_string_keys_it_setup, write_string_keys_run_10000, write_string_keys_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(write_string_keys_100k, nullptr, write_string_keys_it_setup, write_string_keys_run_100000, write_string_keys_it_teardown, nullptr);

// ============================================================================
// Benchmark 5: Sequential string reads
// ============================================================================
void read_string_keys_it_setup(sbf::Benchmark& bm, int nkeys) {
    auto ctx = new ScalarCtx();
    (void)ctx->store.load("bench", ctx->temp.path(), akasha::FileOptions::create_if_missing);

    // Pre-populate
    for (size_t i = 0; i < nkeys; ++i) {
        std::string value = "Value number " + std::to_string(i) + " with some extra padding";
        (void)ctx->store.set<std::string>("bench/str/" + std::to_string(i), value);
    }

    bm.set_local_context(ctx);
}
void read_string_keys_it_setup_1000(sbf::Benchmark& bm) {
    read_string_keys_it_setup(bm, 1000);
}
void read_string_keys_it_setup_10000(sbf::Benchmark& bm) {
    read_string_keys_it_setup(bm, 10000);
}
void read_string_keys_it_setup_100000(sbf::Benchmark& bm) {
    read_string_keys_it_setup(bm, 100000);
}
void read_string_keys_run(sbf::Benchmark& bm, int nkeys) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();

    // Now benchmark reads
    for (size_t i = 0; i < 1000; ++i) {
        auto val = ctx->store.get<std::string>("bench/str/" + std::to_string(rand() % nkeys));
        BENCHMARK_OPERATION;
        (void)val;
    }
}
void read_string_keys_run_1000(sbf::Benchmark& bm) {
    read_string_keys_run(bm, 1000);
}
void read_string_keys_run_10000(sbf::Benchmark& bm) {
    read_string_keys_run(bm, 10000);
}
void read_string_keys_run_100000(sbf::Benchmark& bm) {
    read_string_keys_run(bm, 100000);
}
void read_string_keys_it_teardown(sbf::Benchmark& bm) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    (void)ctx->store.unload("bench");
    delete ctx;
}

BENCHMARK_REGISTER_FULL(read_string_keys_1k, nullptr, read_string_keys_it_setup_1000, read_string_keys_run_1000, read_string_keys_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(read_string_keys_10k, nullptr, read_string_keys_it_setup_10000, read_string_keys_run_10000, read_string_keys_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(read_string_keys_100k, nullptr, read_string_keys_it_setup_100000, read_string_keys_run_100000, read_string_keys_it_teardown, nullptr);

// ============================================================================
// Benchmark 6: Vector writes
// ============================================================================
void write_vector_it_setup(sbf::Benchmark& bm) {
    auto ctx = new VectorCtx();
    
    (void)ctx->store.load("bench", ctx->temp.path(), akasha::FileOptions::create_if_missing);

    for (size_t i = 0; i < 1000; ++i) {    
        ctx->vec.push_back(static_cast<double>(i));
    }
    bm.set_local_context(ctx);
}

void write_vector_run(sbf::Benchmark& bm, int nkeys) {
    auto& ctx = bm.get_local_context<VectorCtx*>();
    for (size_t i = 0; i < nkeys; ++i) {
        (void)ctx->store.set<std::vector<double>>("bench/vector/" + std::to_string(i), ctx->vec);
        BENCHMARK_OPERATION;
    }
}
void write_vector_run_1000(sbf::Benchmark& bm) {
    write_vector_run(bm, 1000);
}
void write_vector_run_10000(sbf::Benchmark& bm) {
    write_vector_run(bm, 10000);
}
void write_vector_run_100000(sbf::Benchmark& bm) {
    write_vector_run(bm, 100000);
}
void write_vector_it_teardown(sbf::Benchmark& bm) {
    auto& ctx = bm.get_local_context<VectorCtx*>();
    (void)ctx->store.unload("bench");
    delete ctx;
}

BENCHMARK_REGISTER_FULL(write_vector_1k, nullptr, write_vector_it_setup, write_vector_run_1000, write_vector_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(write_vector_10k, nullptr, write_vector_it_setup, write_vector_run_10000, write_vector_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(write_vector_100k, nullptr, write_vector_it_setup, write_vector_run_100000, write_vector_it_teardown, nullptr);

// ============================================================================
// Benchmark 7: Vector reads
// ============================================================================
void read_vector_it_setup(sbf::Benchmark& bm, int nkeys) {
    auto ctx = new ScalarCtx();
    (void)ctx->store.load("bench", ctx->temp.path(), akasha::FileOptions::create_if_missing);

    // Pre-populate
    for (size_t i = 0; i < nkeys; ++i) {
        std::vector<double> vec({static_cast<double>(i), static_cast<double>(i) * 2, static_cast<double>(i) * 3});
        (void)ctx->store.set<std::vector<double>>("bench/vector/" + std::to_string(i), vec);
    }

    bm.set_local_context(ctx);
}
void read_vector_it_setup_1000(sbf::Benchmark& bm) {
    read_vector_it_setup(bm, 1000);
}
void read_vector_it_setup_10000(sbf::Benchmark& bm) {
    read_vector_it_setup(bm, 10000);
}
void read_vector_it_setup_100000(sbf::Benchmark& bm) {
    read_vector_it_setup(bm, 100000);
}
void read_vector_run(sbf::Benchmark& bm, int nkeys) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    // Now benchmark reads
    for (size_t i = 0; i < nkeys; ++i) {
        auto val = ctx->store.get<std::vector<double>>("bench/vector/" + std::to_string(rand() % nkeys));
        BENCHMARK_OPERATION;
        (void)val;
    }
}
void read_vector_run_1000(sbf::Benchmark& bm) {
    read_vector_run(bm, 1000);
}
void read_vector_run_10000(sbf::Benchmark& bm) {
    read_vector_run(bm, 10000);
}
void read_vector_run_100000(sbf::Benchmark& bm) {
    read_vector_run(bm, 100000);
}
void read_vector_it_teardown(sbf::Benchmark& bm) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    (void)ctx->store.unload("bench");
    delete ctx;
}

BENCHMARK_REGISTER_FULL(read_vector_1k, nullptr, read_vector_it_setup_1000, read_vector_run_1000, read_vector_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(read_vector_10k, nullptr, read_vector_it_setup_10000, read_vector_run_10000, read_vector_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(read_vector_100k, nullptr, read_vector_it_setup_100000, read_vector_run_100000, read_vector_it_teardown, nullptr);

// ============================================================================
// Benchmark 8: Simple serializable writes
// ============================================================================
void write_serializable_point_it_setup(sbf::Benchmark& bm) {
    auto ctx = new ScalarCtx();
    (void)ctx->store.load("bench", ctx->temp.path(), akasha::FileOptions::create_if_missing);
    bm.set_local_context(ctx);
}
void write_serializable_point_run(sbf::Benchmark& bm, int nkeys) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();

    for (size_t i = 0; i < nkeys; ++i) {
        Point p{static_cast<double>(i), static_cast<double>(i) * 2, static_cast<double>(i) * 3};
        (void)ctx->store.set<Point>("bench/point/" + std::to_string(i), p);
        BENCHMARK_OPERATION;
    }
}
void write_serializable_point_run_1000(sbf::Benchmark& bm) {
    write_serializable_point_run(bm, 1000);
}
void write_serializable_point_run_10000(sbf::Benchmark& bm) {
    write_serializable_point_run(bm, 10000);
}
void write_serializable_point_run_100000(sbf::Benchmark& bm) {
    write_serializable_point_run(bm, 100000);
}
void write_serializable_point_it_teardown(sbf::Benchmark& bm) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    (void)ctx->store.unload("bench");
    delete ctx;
}

BENCHMARK_REGISTER_FULL(write_serializable_point_1k, nullptr, write_serializable_point_it_setup, write_serializable_point_run_1000, write_serializable_point_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(write_serializable_point_10k, nullptr, write_serializable_point_it_setup, write_serializable_point_run_10000, write_serializable_point_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(write_serializable_point_100k, nullptr, write_serializable_point_it_setup, write_serializable_point_run_100000, write_serializable_point_it_teardown, nullptr);

// ============================================================================
// Benchmark 9: Simple serializable reads
// ============================================================================
void read_serializable_point_it_setup(sbf::Benchmark& bm, int nkeys) {
    auto ctx = new ScalarCtx();
    (void)ctx->store.load("bench", ctx->temp.path(), akasha::FileOptions::create_if_missing);

    // Pre-populate
    for (size_t i = 0; i < nkeys; ++i) {
        Point p{static_cast<double>(i), static_cast<double>(i) * 2, static_cast<double>(i) * 3};
        (void)ctx->store.set<Point>("bench/point/" + std::to_string(i), p);
    }

    bm.set_local_context(ctx);
}
void read_serializable_point_it_setup_1000(sbf::Benchmark& bm) {
    read_serializable_point_it_setup(bm, 1000);
}
void read_serializable_point_it_setup_10000(sbf::Benchmark& bm) {
    read_serializable_point_it_setup(bm, 10000);
}
void read_serializable_point_it_setup_100000(sbf::Benchmark& bm) {
    read_serializable_point_it_setup(bm, 100000);
}
void read_serializable_point_run(sbf::Benchmark& bm, int nkeys) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();

    // Now benchmark reads
    for (size_t i = 0; i < nkeys; ++i) {
        auto val = ctx->store.get<Point>("bench/point/" + std::to_string(rand() % nkeys));
        BENCHMARK_OPERATION;
        (void)val;
    }
}
void read_serializable_point_run_1000(sbf::Benchmark& bm) {
    read_serializable_point_run(bm, 1000);
}
void read_serializable_point_run_10000(sbf::Benchmark& bm) {
    read_serializable_point_run(bm, 10000);
}
void read_serializable_point_run_100000(sbf::Benchmark& bm) {
    read_serializable_point_run(bm, 100000);
}
void read_serializable_point_it_teardown(sbf::Benchmark& bm) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    (void)ctx->store.unload("bench");
    delete ctx;
}

BENCHMARK_REGISTER_FULL(read_serializable_point_1k, nullptr, read_serializable_point_it_setup_1000, read_serializable_point_run_1000, read_serializable_point_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(read_serializable_point_10k, nullptr, read_serializable_point_it_setup_10000, read_serializable_point_run_10000, read_serializable_point_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(read_serializable_point_100k, nullptr, read_serializable_point_it_setup_100000, read_serializable_point_run_100000, read_serializable_point_it_teardown, nullptr);

// ============================================================================
// Benchmark 10: Complex serializable writes
// ============================================================================
void write_complex_serializable_it_setup(sbf::Benchmark& bm) {
    auto ctx = new SceneCtx();
    (void)ctx->store.load("bench", ctx->temp.path(), akasha::FileOptions::create_if_missing);
    ctx->scene = Scene{
            "Main scene", true, 42,
            Camera{{0.0, 5.0, -10.0}, {0.0, 0.0, 0.0}, 60.0},
            {0.2, 0.3, 0.5},
            {"outdoor", "winter", "night"}
        };
    bm.set_local_context(ctx);
}
void write_complex_serializable_run(sbf::Benchmark& bm, int nkeys) {
    auto& ctx = bm.get_local_context<SceneCtx*>();

    for (size_t i = 0; i < nkeys; ++i) {
        (void)ctx->store.set<Scene>("bench/scenes/" + std::to_string(i), ctx->scene);
        BENCHMARK_OPERATION;
    }
}
void write_complex_serializable_run_1000(sbf::Benchmark& bm) {
    write_complex_serializable_run(bm, 1000);
}
void write_complex_serializable_run_10000(sbf::Benchmark& bm) {
    write_complex_serializable_run(bm, 10000);
}
void write_complex_serializable_run_100000(sbf::Benchmark& bm) {
    write_complex_serializable_run(bm, 100000);
}
void write_complex_serializable_it_teardown(sbf::Benchmark& bm) {
    auto& ctx = bm.get_local_context<SceneCtx*>();
    (void)ctx->store.unload("bench");
    delete ctx;
}

BENCHMARK_REGISTER_FULL(write_complex_serializable_1k, nullptr, write_complex_serializable_it_setup, write_complex_serializable_run_1000, write_complex_serializable_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(write_complex_serializable_10k, nullptr, write_complex_serializable_it_setup, write_complex_serializable_run_10000, write_complex_serializable_it_teardown, nullptr);
BENCHMARK_REGISTER_FULL(write_complex_serializable_100k, nullptr, write_complex_serializable_it_setup, write_complex_serializable_run_100000, write_complex_serializable_it_teardown, nullptr);

// ============================================================================
// Benchmark 11: Complex serializable reads
// ============================================================================
void read_complex_serializable_setup(sbf::Benchmark& bm, int nkeys) {
    auto ctx = new ScalarCtx();
    (void)ctx->store.load("bench", ctx->temp.path(), akasha::FileOptions::create_if_missing);
    (void)ctx->store.clear();

    // Pre-populate
    Scene scene = Scene{
        "Main scene", true, 42,
        Camera{{0.0, 5.0, -10.0}, {0.0, 0.0, 0.0}, 60.0},
        {0.2, 0.3, 0.5},
        {"outdoor", "winter", "night"}
    };

    for (size_t i = 0; i < nkeys; ++i) {
        bm.set_progress("Setting up", i, nkeys);
        scene.name = "Scene " + std::to_string(i);
        (void)ctx->store.set<Scene>("bench/scenes/" + std::to_string(i), scene);
    }

    (void)ctx->store.unload("bench");
    bm.set_context(ctx);
}
void read_complex_serializable_setup_1000(sbf::Benchmark& bm) {
    read_complex_serializable_setup(bm, 1000);
}
void read_complex_serializable_setup_10000(sbf::Benchmark& bm) {
    read_complex_serializable_setup(bm, 10000);
}
void read_complex_serializable_setup_100000(sbf::Benchmark& bm) {
    read_complex_serializable_setup(bm, 100000);
}
void read_complex_serializable_it_setup(sbf::Benchmark& bm) {
    auto& ctx = bm.get_context<ScalarCtx*>();
    auto local_ctx = new ScalarCtx();

    local_ctx->temp = TempFile(ctx->temp);
    (void)local_ctx->store.load("bench", local_ctx->temp.path());
    bm.set_local_context(local_ctx);
}
void read_complex_serializable_run(sbf::Benchmark& bm, int nkeys) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();

    // Now benchmark reads
    for (size_t i = 0; i < nkeys; ++i) {
        auto val = ctx->store.get<Scene>("bench/scenes/" + std::to_string(rand() % nkeys));
        BENCHMARK_OPERATION;
        (void)val;
    }
}
void read_complex_serializable_run_1000(sbf::Benchmark& bm) {
    read_complex_serializable_run(bm, 1000);
}
void read_complex_serializable_run_10000(sbf::Benchmark& bm) {
    read_complex_serializable_run(bm, 10000);
}
void read_complex_serializable_run_100000(sbf::Benchmark& bm) {
    read_complex_serializable_run(bm, 100000);
}
void read_complex_serializable_it_teardown(sbf::Benchmark& bm) {
    auto& ctx = bm.get_local_context<ScalarCtx*>();
    (void)ctx->store.unload("bench");
    delete ctx;
}
void read_complex_serializable_teardown(sbf::Benchmark& bm) {
    auto& ctx = bm.get_context<ScalarCtx*>();
    (void)ctx->store.unload("bench");
    delete ctx;
}

BENCHMARK_REGISTER_FULL(read_complex_serializable_1k, read_complex_serializable_setup_1000, read_complex_serializable_it_setup, read_complex_serializable_run_1000, read_complex_serializable_it_teardown, read_complex_serializable_teardown);
BENCHMARK_REGISTER_FULL(read_complex_serializable_10k, read_complex_serializable_setup_10000, read_complex_serializable_it_setup, read_complex_serializable_run_10000, read_complex_serializable_it_teardown, read_complex_serializable_teardown);
BENCHMARK_REGISTER_FULL(read_complex_serializable_100k, read_complex_serializable_setup_100000, read_complex_serializable_it_setup, read_complex_serializable_run_100000, read_complex_serializable_it_teardown, read_complex_serializable_teardown);

// ============================================================================
// Main: Run all benchmarks
// ============================================================================
int main() {
    sbf::Runner runner;
    auto results = runner.set_default_iterations(10)
                         .set_default_warmup(0)
                         .set_default_threads(1)
                         .add_all()
                         .run();

    // Print results using default console reporter
    sbf::reporters::ConsoleReporter{}.report(results, std::cout);  
    
    try {
        std::ofstream csv("akasha_benchmark.csv");
        if (!csv) throw std::runtime_error("Failed to open akasha_benchmark.csv");
        sbf::reporters::CSVReporter{}.report(results, csv);
        std::cout << "CSV report written to: akasha_benchmark.csv\n";
    } catch (const std::exception& e) {
        std::cerr << "Error exporting results: " << e.what() << "\n";
    }
    std::cout << "\n✓ Benchmarks completed\n";

    return 0;
}
