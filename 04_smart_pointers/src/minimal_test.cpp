#include <iostream>

// Test 1: Include just unique_ptr
#include "unique_ptr.hpp"

struct Widget {
    int id;
    Widget(int i) : id(i) {
        std::cout << "Widget " << id << " created\n";
    }
    ~Widget() {
        std::cout << "Widget " << id << " destroyed\n";
    }
};

int main() {
    std::cout << "=== Starting Minimal Test ===\n";
    
    try {
        std::cout << "\n--- Test 1: unique_ptr ---\n";
        {
            auto ptr = hft::smart::make_unique<Widget>(1);
            std::cout << "Created unique_ptr\n";
        }
        std::cout << "Destroyed unique_ptr\n";
        
        std::cout << "\n=== All Tests Passed ===\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception\n";
        return 1;
    }
}