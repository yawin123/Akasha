#pragma once

#include <string_view>

namespace akasha {

[[nodiscard]] std::string_view version() noexcept;
[[nodiscard]] int add(int a, int b) noexcept;

}  // namespace akasha
