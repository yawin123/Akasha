#include <iostream>
#include <vector>
#include <iomanip>
#include "akasha.hpp"

int main() {
    akasha::Store store;

    // Load a file (created if it doesn't exist)
    auto status = store.load("sensors", "/tmp/sensors.db", 
                            akasha::FileOptions::create_if_missing | 
                            akasha::FileOptions::migrate_if_incompatible);
    if (status != akasha::Status::ok) {
        std::cerr << "Error loading sensors database\n";
        return 1;
    }

    std::cout << "=== Akasha Vector Storage Example ===\n\n";

    // ======================================================================
    // Store vectors of different scalar types
    // ======================================================================
    
    std::cout << "1. Storing vector of integers...\n";
    std::vector<int> temperatures = {22, 23, 21, 24, 22, 23};
    status = store.set<std::vector<int>>("sensors.temperature.readings", temperatures);
    if (status != akasha::Status::ok) {
        std::cerr << "Error storing temperatures\n";
        return 1;
    }
    std::cout << "   Stored " << temperatures.size() << " temperature readings\n\n";

    std::cout << "2. Storing vector of doubles...\n";
    std::vector<double> humidity = {45.2, 46.8, 44.5, 47.1, 45.9, 46.3};
    status = store.set<std::vector<double>>("sensors.humidity.readings", humidity);
    if (status != akasha::Status::ok) {
        std::cerr << "Error storing humidity\n";
        return 1;
    }
    std::cout << "   Stored " << humidity.size() << " humidity readings\n\n";

    std::cout << "3. Storing vector of booleans...\n";
    std::vector<bool> alarm_flags = {false, false, true, false, false, true};
    status = store.set<std::vector<bool>>("sensors.alarms.triggered", alarm_flags);
    if (status != akasha::Status::ok) {
        std::cerr << "Error storing alarms\n";
        return 1;
    }
    std::cout << "   Stored " << alarm_flags.size() << " alarm flags\n\n";

    // ======================================================================
    // Store an empty vector
    // ======================================================================
    
    std::cout << "4. Storing empty vector...\n";
    std::vector<uint64_t> empty_metrics;
    status = store.set<std::vector<uint64_t>>("sensors.metrics.historical", empty_metrics);
    if (status != akasha::Status::ok) {
        std::cerr << "Error storing empty vector\n";
        return 1;
    }
    std::cout << "   Stored empty vector for future use\n\n";

    // ======================================================================
    // Retrieve and display vectors
    // ======================================================================
    
    std::cout << "5. Retrieving stored vectors...\n";
    
    auto temps = store.get<std::vector<int>>("sensors.temperature.readings");
    if (temps.has_value()) {
        std::cout << "   Temperatures: ";
        for (size_t i = 0; i < temps->size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << temps->at(i) << "°C";
        }
        std::cout << "\n";
    }
    
    auto humids = store.get<std::vector<double>>("sensors.humidity.readings");
    if (humids.has_value()) {
        std::cout << "   Humidity: ";
        for (size_t i = 0; i < humids->size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << std::fixed << std::setprecision(1) << humids->at(i) << "%";
        }
        std::cout << "\n";
    }
    
    auto alarms = store.get<std::vector<bool>>("sensors.alarms.triggered");
    if (alarms.has_value()) {
        std::cout << "   Alarm flags: ";
        for (size_t i = 0; i < alarms->size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << (alarms->at(i) ? "TRIGGERED" : "OK");
        }
        std::cout << "\n";
    }
    
    auto empty = store.get<std::vector<uint64_t>>("sensors.metrics.historical");
    if (empty.has_value()) {
        std::cout << "   Historical metrics: " << (empty->empty() ? "empty" : "has data") << "\n";
    }
    std::cout << "\n";

    // ======================================================================
    // Coexistence: scalars, strings, and vectors in the same dataset
    // ======================================================================
    
    std::cout << "6. Storing mixed types in the same dataset...\n";
    
    // Scalar
    status = store.set<int64_t>("sensors.config.sensor_id", 42);
    if (status != akasha::Status::ok) {
        std::cerr << "Error storing sensor_id\n";
        return 1;
    }
    
    // String
    status = store.set<std::string>("sensors.config.location", "Building A - Floor 3");
    if (status != akasha::Status::ok) {
        std::cerr << "Error storing location\n";
        return 1;
    }
    
    // Vector
    std::vector<int> calibration_points = {0, 25, 50, 75, 100};
    status = store.set<std::vector<int>>("sensors.config.calibration_points", calibration_points);
    if (status != akasha::Status::ok) {
        std::cerr << "Error storing calibration points\n";
        return 1;
    }
    
    std::cout << "   All types coexist successfully\n\n";

    // ======================================================================
    // Retrieve mixed types (verify no interference)
    // ======================================================================
    
    std::cout << "7. Retrieving mixed types...\n";
    
    auto sensor_id = store.get<int64_t>("sensors.config.sensor_id");
    if (sensor_id.has_value()) {
        std::cout << "   Sensor ID: " << sensor_id.value() << "\n";
    }
    
    auto location = store.get<std::string>("sensors.config.location");
    if (location.has_value()) {
        std::cout << "   Location: " << location.value() << "\n";
    }
    
    auto calib = store.get<std::vector<int>>("sensors.config.calibration_points");
    if (calib.has_value()) {
        std::cout << "   Calibration points: ";
        for (size_t i = 0; i < calib->size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << calib->at(i);
        }
        std::cout << "\n";
    }
    std::cout << "\n";

    // ======================================================================
    // Large vector (1000 elements)
    // ======================================================================
    
    std::cout << "8. Storing and retrieving large vector...\n";
    
    std::vector<double> large_dataset;
    for (int i = 0; i < 1000; ++i) {
        large_dataset.push_back(static_cast<double>(i) * 1.5);
    }
    
    status = store.set<std::vector<double>>("sensors.data.large_dataset", large_dataset);
    if (status != akasha::Status::ok) {
        std::cerr << "Error storing large dataset\n";
        return 1;
    }
    std::cout << "   Stored " << large_dataset.size() << " elements\n";
    
    auto retrieved_large = store.get<std::vector<double>>("sensors.data.large_dataset");
    if (retrieved_large.has_value()) {
        std::cout << "   Retrieved " << retrieved_large->size() << " elements\n";
        std::cout << "   First: " << std::setprecision(1) << retrieved_large->at(0) << "\n";
        std::cout << "   Last:  " << retrieved_large->at(retrieved_large->size() - 1) << "\n";
    }
    std::cout << "\n";

    // ======================================================================
    // Lazy initialization with getorset
    // ======================================================================
    
    std::cout << "9. Lazy initialization (getorset)...\n";
    
    std::vector<int> default_settings = {1, 2, 3, 4, 5};
    
    // First call: doesn't exist, sets and returns default
    auto settings1 = store.getorset<std::vector<int>>("sensors.settings.defaults", default_settings);
    if (settings1.has_value()) {
        std::cout << "   First call (didn't exist): stored and retrieved defaults\n";
    }
    
    // Second call: exists, returns the stored value (not the new default)
    auto settings2 = store.getorset<std::vector<int>>("sensors.settings.defaults", 
                                                      std::vector<int>{10, 20, 30});
    if (settings2.has_value()) {
        std::cout << "   Second call (already exists): retrieved original settings\n";
        std::cout << "   Values: ";
        for (size_t i = 0; i < settings2->size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << settings2->at(i);
        }
        std::cout << "\n";
    }
    std::cout << "\n";

    // ======================================================================
    // Verify navigation hints
    // ======================================================================
    
    std::cout << "10. Checking hierarchical navigation...\n";
    if (store.has("sensors.temperature")) {
        std::cout << "    ✓ 'sensors.temperature' exists (intermediate level)\n";
    }
    if (store.has("sensors.temperature.readings")) {
        std::cout << "    ✓ 'sensors.temperature.readings' exists (vector stored)\n";
    }
    auto view = store.get<akasha::Store::DatasetView>("sensors.config");
    if (view.has_value()) {
        auto keys = view->keys();
        std::cout << "    Keys under 'sensors.config': ";
        for (const auto& key : keys) {
            std::cout << key << " ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";

    // ======================================================================
    // Bonus: Vector of strings
    // ======================================================================
    
    std::cout << "11. Vector of strings (bonus)...\n";
    
    std::vector<std::string> log_entries = {
        "System started",
        "User login successful",
        "Data sync completed",
        "Configuration updated",
        "Backup created"
    };
    
    status = store.set<std::vector<std::string>>("sensors.logs.entries", log_entries);
    if (status != akasha::Status::ok) {
        std::cerr << "Error storing log entries\n";
        return 1;
    }
    std::cout << "    Stored " << log_entries.size() << " log entries\n";
    
    auto logs = store.get<std::vector<std::string>>("sensors.logs.entries");
    if (logs.has_value()) {
        std::cout << "    Retrieved logs:\n";
        for (size_t i = 0; i < logs->size(); ++i) {
            std::cout << "      [" << i << "] " << logs->at(i) << "\n";
        }
    }
    std::cout << "\n";

    // Cleanup
    store.unload("sensors");

    std::cout << "=== Example Complete ===\n";
    std::cout << "Data persisted to /tmp/sensors.db\n";
    
    return 0;
}
