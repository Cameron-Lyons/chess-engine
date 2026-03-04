#include "protocol/uci.h"

#include <exception>
#include <iostream>

int main() {
    try {
        return runUCIEngine();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
}
