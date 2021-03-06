#pragma once

#include <chrono>


namespace dal {

    double get_cur_sec();

    void sleep_for(const double seconds);


    class Timer {

    private:
        std::chrono::steady_clock::time_point m_last_checked = std::chrono::steady_clock::now();

    public:
        void check();

        double get_elapsed() const;

        double check_get_elapsed();

    protected:
        auto& last_checked() const {
            return this->m_last_checked;
        }

    };


    class TimerThatCaps : public Timer {

    private:
        uint32_t m_desired_delta_microsec = 0;

    public:
        bool has_elapsed(const double sec) const;

        double check_get_elapsed_cap_fps();

        void set_fps_cap(const uint32_t v);

    private:
        void wait_to_cap_fps();

    };

}
