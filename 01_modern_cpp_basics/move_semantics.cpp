#include <iostream>
#include <string>
#include <vector>
#include <type_traits>
#include <utility>

struct Packet {
	std::string payload;

	Packet(const std::string& p) : payload(p) {
		std::cout << "Copy constructor: " << payload << std::endl;
	}

	Packet(std::string&& p) : payload(std::move(p)) {
		std::cout << "Move constructor: " << payload << std::endl;
	}

	Packet(const Packet& other) : payload(other.payload) {
		std::cout << "Copy constructor (from Packet): " << payload << std::endl;
	}

	Packet(Packet&& other) noexcept : payload(std::move(other.payload)) {
		std::cout << "Move constructor (from Packet): " << payload << std::endl;
	}

	~Packet() {
		std::cout << "Destructor: " << payload << std::endl;
	}
};

Packet create_packet(bool use_rvo) {
	if (use_rvo) {
		return Packet("RVO");  // Return Value Optimization 
	}
	else {
		Packet p("No RVO");  // Named Return Value (NRVO)
		return p;
	}
}

void test_vector_emplace_vs_push() {
	std::vector<Packet> vec;
	vec.reserve(2);

	std::cout << "\n[emplace_back]:\n";
	vec.emplace_back("emplace");  // Perfect forwarding

	std::cout << "\n[push_back (temporary)]:\n";
	vec.push_back(Packet("push"));
}

void test_traits() {
	std::cout << "\n[Traits]: \n";
	std::cout << "is_nothrow_move_constructible<Packet>: "
			  << std::boolalpha
			  << std::is_nothrow_move_constructible<Packet>::value
			  << std::endl;

	Packet src("conditional_move");
	Packet dst = std::move_if_noexcept(src);  // Move vs copy fallback
}

int main() {
	std::cout << "[RVO]:" << std::endl;
	Packet p1 = create_packet(true);

	std::cout << "\n[NRVO]:\n";
	Packet p2 = create_packet(false);

	test_vector_emplace_vs_push();

	test_traits();
}