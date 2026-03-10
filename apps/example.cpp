#include <iostream>

#include "akasha.hpp"

int main() {
    std::cout << "akasha version: " << akasha::version() << '\n';
    std::cout << "2 + 3 = " << akasha::add(2, 3) << '\n';
    return 0;
}
