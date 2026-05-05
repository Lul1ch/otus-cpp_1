#include "lib.h"

#include <iostream>

// Кто прочитал, тот зайка!

int main(int, char **) {
	std::cout << "Version: " << version() << std::endl;
	std::cout << "Hello, world!" << std::endl;
	return 0;
}
