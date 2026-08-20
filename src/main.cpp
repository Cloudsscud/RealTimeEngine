#include <first_app.h>

// std
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
	czx::FirstAPP app{};

	try {
		app.run();
	}
	catch (const std::exception& e){
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;	// cstdlib
	}

	return EXIT_SUCCESS;
}