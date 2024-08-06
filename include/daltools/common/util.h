#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <vector>

#include <sung/general/time.hpp>


namespace dal {

    class TimerThatCaps : public sung::MonotonicRealtimeTimer {

    public:
        double check_get_elapsed_cap_fps();
        void set_fps_cap(const uint32_t v);

    private:
        void wait_to_cap_fps();

        uint32_t desired_delta_ = 0;  // In microseconds
    };


    namespace fs = std::filesystem;

    std::optional<fs::path> find_git_repo_root(const fs::path& start_path);
    std::vector<uint8_t> read_file(const fs::path& path);

}  // namespace dal
