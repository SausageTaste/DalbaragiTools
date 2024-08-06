#include "daltools/common/util.h"

#include <fstream>
#include <thread>


namespace {

    constexpr unsigned MISCROSEC_PER_SEC = 1000000;
    constexpr unsigned NANOSEC_PER_SEC = 1000000000;

    namespace chr = std::chrono;
    using SteadyClock = chr::steady_clock;


    void sleep_hot_until(const SteadyClock::time_point& until) {
        while (SteadyClock::now() < until) {
        }
    }

    void sleep_cold_until(const SteadyClock::time_point& until) {
        std::this_thread::sleep_until(until);
    }

    void sleep_hybrid_until(const SteadyClock::time_point& until) {
        const auto dur = until - SteadyClock::now();
        const auto dur_in_ms = chr::duration_cast<chr::microseconds>(dur);
        std::this_thread::sleep_for(chr::microseconds{ dur_in_ms.count() / 4 });
        ::sleep_hot_until(until);
    }

}  // namespace


// Header functions
namespace dal {

    std::optional<fs::path> find_git_repo_root(const fs::path& start_path) {
        auto current_dir = start_path;

        for (int i = 0; i < 10; ++i) {
            for (const auto& entry : fs::directory_iterator(current_dir)) {
                if (entry.path().filename().u8string() == ".git") {
                    return current_dir;
                }
            }
            current_dir = current_dir.parent_path();
        }

        return std::nullopt;
    }

    std::vector<uint8_t> read_file(const fs::path& path) {
        std::vector<uint8_t> content;

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return content;

        content.assign(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );
        return content;
    }

}  // namespace dal


// TimerThatCaps
namespace dal {

    double TimerThatCaps::check_get_elapsed_cap_fps() {
        this->wait_to_cap_fps();
        return this->check_get_elapsed();
    }

    void TimerThatCaps::set_fps_cap(const uint32_t v) {
        if (0 != v)
            desired_delta_ = ::MISCROSEC_PER_SEC / v;
        else
            desired_delta_ = 0;
    }

    // Private

    void TimerThatCaps::wait_to_cap_fps() {
        const auto delta = std::chrono::microseconds{ desired_delta_ };
        const auto wake_time = this->last_checked() + delta;
        ::sleep_hybrid_until(wake_time);
    }

}  // namespace dal
