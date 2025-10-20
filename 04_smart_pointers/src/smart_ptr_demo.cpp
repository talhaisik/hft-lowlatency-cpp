#include <iostream>
#include <string>
#include <vector>

// Include all smart pointer implementations
#include "unique_ptr.hpp"
#include "shared_ptr.hpp"
#include "weak_ptr.hpp"

using namespace hft::smart;

// Test Classes

struct Widget {
    int id;
    std::string name;

    Widget(int i, std::string n) : id(i), name(std::move(n)) {
        std::cout << "Widget " << id << " (" << name << ") constructed\n";
    }

    ~Widget() {
        std::cout << "Widget " << id << " (" << name << ") destructed\n";
    }

    void use() {
        std::cout << "Using Widget " << id << " (" << name << ")\n";
    }
};

// unique_ptr Examples

void demo_unique_ptr() {
    std::cout << "\n|------------------------------------------|\n";
    std::cout << "|        unique_ptr Demonstration          |\n";
    std::cout << "|------------------------------------------|\n";

    std::cout << "\n--- Example 1: Basic Usage ---\n";
    {
        auto widget = make_unique<Widget>(1, "Alpha");
        widget->use();
    } // Widget automatically destroyed

    std::cout << "\n--- Example 2: Move Semantics ---\n";
    {
        auto widget1 = make_unique<Widget>(2, "Beta");
        std::cout << "widget1 owns: " << widget1.get() << "\n";

        auto widget2 = std::move(widget1);
        std::cout << "After move:\n";
        std::cout << "  widget1 owns: " << widget1.get() << " (nullptr)\n";
        std::cout << "  widget2 owns: " << widget2.get() << "\n";
    }

    std::cout << "\n--- Example 3: Release and Reset ---\n";
    {
        auto widget = make_unique<Widget>(3, "Gamma");

        Widget* raw = widget.release();
        std::cout << "After release, widget owns: " << widget.get() << "\n";

        widget.reset(new Widget(4, "Delta"));
        delete raw; // Manual cleanup of released pointer
    }

    std::cout << "\n--- Example 4: Size Comparison ---\n";
    std::cout << "sizeof(Widget*):       " << sizeof(Widget*) << " bytes\n";
    std::cout << "sizeof(UniquePtr):     " << sizeof(UniquePtr<Widget>) << " bytes\n";
    std::cout << "Zero overhead! \n";
}

// shared_ptr Examples

void demo_shared_ptr() {
    std::cout << "\n|------------------------------------------|\n";
    std::cout << "|        shared_ptr Demonstration          |\n";
    std::cout << "|------------------------------------------|\n";

    std::cout << "\n--- Example 1: Shared Ownership ---\n";
    {
        auto ptr1 = make_shared<Widget>(10, "Shared-A");
        std::cout << "ptr1 use_count: " << ptr1.use_count() << "\n";

        {
            auto ptr2 = ptr1;
            auto ptr3 = ptr1;
            std::cout << "After creating ptr2 and ptr3:\n";
            std::cout << "  use_count: " << ptr1.use_count() << "\n";
        }

        std::cout << "After inner scope:\n";
        std::cout << "  use_count: " << ptr1.use_count() << "\n";
    }

    std::cout << "\n--- Example 2: Containers ---\n";
    {
        auto resource = make_shared<Widget>(11, "Shared-B");
        std::cout << "Initial use_count: " << resource.use_count() << "\n";

        std::vector<SharedPtr<Widget>> vec;
        vec.push_back(resource);
        vec.push_back(resource);
        vec.push_back(resource);

        std::cout << "After adding to vector 3 times:\n";
        std::cout << "  use_count: " << resource.use_count() << "\n";

        vec.clear();
        std::cout << "After clearing vector:\n";
        std::cout << "  use_count: " << resource.use_count() << "\n";
    }

    std::cout << "\n--- Example 3: Size Comparison ---\n";
    std::cout << "sizeof(Widget*):       " << sizeof(Widget*) << " bytes\n";
    std::cout << "sizeof(SharedPtr):     " << sizeof(SharedPtr<Widget>) << " bytes\n";
    std::cout << "Overhead: control block pointer\n";
}

// weak_ptr Examples

struct Node {
    std::string name;
    SharedPtr<Node> next;
    WeakPtr<Node> prev;  // Break cycle with weak_ptr

    Node(std::string n) : name(std::move(n)) {
        std::cout << "Node " << name << " created\n";
    }

    ~Node() {
        std::cout << "Node " << name << " destroyed\n";
    }
};

void demo_weak_ptr() {
    std::cout << "\n|------------------------------------------|\n";
    std::cout << "|        weak_ptr Demonstration          |\n";
    std::cout << "|------------------------------------------|\n";

    std::cout << "\n--- Example 1: Basic Usage ---\n";
    {
        WeakPtr<Widget> weak;

        {
            auto shared = make_shared<Widget>(20, "Observed");
            std::cout << "shared use_count: " << shared.use_count() << "\n";

            weak = shared;
            std::cout << "After creating weak:\n";
            std::cout << "  shared use_count: " << shared.use_count() << "\n";
            std::cout << "  weak expired: " << weak.expired() << "\n";

            if (auto locked = weak.lock()) {
                std::cout << "Successfully locked! Widget " << locked->id << "\n";
            }
        }

        std::cout << "\nAfter shared_ptr destroyed:\n";
        std::cout << "  weak expired: " << weak.expired() << "\n";

        if (auto locked = weak.lock()) {
            std::cout << "Locked successfully\n";
        }
        else {
            std::cout << "Cannot lock - object destroyed!\n";
        }
    }

    std::cout << "\n--- Example 2: Breaking Circular References ---\n";
    {
        auto node1 = make_shared<Node>("A");
        auto node2 = make_shared<Node>("B");

        node1->next = node2;  // Forward: owns
        node2->prev = node1;  // Back: observes (breaks cycle!)

        std::cout << "node1 use_count: " << node1.use_count() << "\n";
        std::cout << "node2 use_count: " << node2.use_count() << "\n";
        std::cout << "Leaving scope...\n";
    }
    std::cout << "Both nodes properly destroyed (no leak)!\n";
}

// Comparison

void demo_comparison() {
    std::cout << "\n|----------------------------------------------------|\n";
    std::cout << "|             Smart Pointer Comparison               |\n";
    std::cout << "|----------------------------------------------------|\n";

    std::cout << "| Feature         | unique   | shared     | weak     |\n";
    std::cout << "|-----------------|----------|------------|----------|\n";
    std::cout << "| Ownership       | Exclusive| Shared     | None     |\n";
    std::cout << "| Copy            | No       | Yes        | Yes      |\n";
    std::cout << "| Move            | Yes      | Yes        | Yes      |\n";
    std::cout << "| Size (bytes)    | " << sizeof(UniquePtr<int>) << "       | "<< sizeof(SharedPtr<int>) << "          | " << sizeof(WeakPtr<int>) << "        |\n";
    std::cout << "| Ref counting    | No       | Yes (atom) | Weak cnt |\n";
    std::cout << "| Use case        | Default  | Multi-own  | Observer |\n";
    std::cout << "|-----------------|----------|------------|----------|\n";
}

int main() {
    std::cout << "  Smart Pointers Deep Dive - Demonstration   \n";

    try {
        demo_unique_ptr();
        demo_shared_ptr();
        demo_weak_ptr();
        demo_comparison();

        std::cout << "\n  All Demonstrations Completed Successfully  \n";

        return 0;

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}