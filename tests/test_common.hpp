#pragma once

#include "../include/akasha.hpp"
#include <filesystem>

namespace fs = std::filesystem;

// Common helper for temporary file cleanup
class TempFile {
public:
    TempFile(const std::string& path = "test_store.mmap") : path_(path) { (void)fs::remove(path_); }
    ~TempFile() { (void)fs::remove(path_); }
    const std::string& path() const { return path_; }
private:
    std::string path_;
};
