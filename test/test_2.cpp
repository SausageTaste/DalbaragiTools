#include <iostream>

#include "hydrogen.h"


int main() {
    std::cout << "Test 2" << std::endl;

    if (hydro_init() != 0) {
        return 1;
    }

    return 0;
}
