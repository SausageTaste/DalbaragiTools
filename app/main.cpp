#include <iostream>
#include <string>
#include <unordered_map>

#include <spdlog/fmt/fmt.h>

#include "work_functions.hpp"


int main(int argc, char* argv[]) try {
    using namespace std::string_literals;

    std::unordered_map<std::string, dal::work_func_t> arg_map{
        { "key"s, dal::work_key },
        { "keygen"s, dal::work_keygen },
        { "compile"s, dal::work_compile },
        { "bundle"s, dal::work_bundle },
        { "bundleview"s, dal::work_bundle_view },
        { "extract"s, dal::work_extract },
        { "batch"s, dal::work_batch },
    };

    if (argc < 2)
        throw std::runtime_error{ "Operation must be specified" };

    auto found = arg_map.find(argv[1]);
    if (found != arg_map.end())
        found->second(argc, argv);
    else {
        std::string keys;
        for (const auto& pair : arg_map) {
            if (!keys.empty())
                keys += ", ";
            keys += pair.first;
        }
        throw std::runtime_error{ fmt::format(
            "Unknown operation ({}). It must be one of {{ {} }}", argv[1], keys
        ) };
    }

    return 0;
} catch (const std::runtime_error& e) {
    std::cout << "\nstd::runtime_error: " << e.what() << std::endl;
    return 1;
} catch (const std::exception& e) {
    std::cout << "\nstd::exception: " << e.what() << std::endl;
    return 1;
} catch (...) {
    std::cout << "\nunknown object thrown: " << std::endl;
    return 1;
}
