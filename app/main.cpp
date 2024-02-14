#include <iostream>
#include <string>
#include <unordered_map>

#include <fmt/format.h>

#include "work_functions.hpp"


int main(int argc, char* argv[]) try {
    using namespace std::string_literals;

    std::unordered_map<std::string, dal::work_func_t> arg_map{
        { "key"s, dal::work_key },
        { "keygen"s, dal::work_keygen },
        { "compile"s, dal::work_compile },
    };

    if (argc < 2)
        throw std::runtime_error{ "Operation must be specified" };

    auto found = arg_map.find(argv[1]);
    if (found != arg_map.end())
        found->second(argc, argv);
    else
        throw std::runtime_error{ "Unknown operation ("s + argv[1] + "). It must be one of { key, keygen, compile }" };
}
catch (const std::runtime_error& e) {
    std::cout << "\nstd::runtime_error: " << e.what() << std::endl;
}
catch (const std::exception& e) {
    std::cout << "\nstd::exception: " << e.what() << std::endl;
}
catch (...) {
    std::cout << "\nunknown object thrown: " << std::endl;
}
