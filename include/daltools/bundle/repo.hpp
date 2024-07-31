#pragma once

#include <string>
#include <unordered_map>
#include <vector>


namespace dal {

    class BundleRepository {

    public:
        BundleRepository();
        ~BundleRepository();

        bool notify(const std::string& name, const std::vector<uint8_t>& data);

    private:
        struct Record;
        std::unordered_map<std::string, Record> records_;
    };

}  // namespace dal
