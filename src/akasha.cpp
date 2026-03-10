#include "akasha.hpp"

namespace akasha {

std::string_view version() noexcept {
    return "0.1.0";
}

int add(int a, int b) noexcept {
    return a + b;
}

}  // namespace akasha
