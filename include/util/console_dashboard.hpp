#pragma once

#include "../kernel/scheduler.hpp"
#include "../kernel/task.hpp"
#include <chrono>
#include <map>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>

namespace edurtos
{
    namespace util
    {

        // Color codes for Windows console
        enum class ConsoleColor
        {
            BLACK = 0,
            BLUE = 1,
            GREEN = 2,
            CYAN = 3,
            RED = 4,
            MAGENTA = 5,
            BROWN = 6,
            LIGHT_GRAY = 7,
            DARK_GRAY = 8,
            LIGHT_BLUE = 9,
            LIGHT_GREEN = 10,
            LIGHT_CYAN = 11,
            LIGHT_RED = 12,
            LIGHT_MAGENTA = 13,
            YELLOW = 14,
            WHITE = 15
        };

        class ConsoleDashboard
        {
        public:
            // Constructor takes a reference to the scheduler
            ConsoleDashboard(Scheduler &scheduler);
            ~ConsoleDashboard();

            // Start/stop the dashboard
            void start();
            void stop();

            // Set refresh rate (default 250ms)
            void setRefreshRate(std::chrono::milliseconds rate);

            // Manual refresh
            void refresh();

            // Configure dashboard
            void showCpuUtilization(bool show);
            void showDeadlines(bool show);
            void showTaskDetails(bool show);
            void showProgressBars(bool show);

        private:
            // Reference to scheduler
            Scheduler &scheduler_;

            // Dashboard settings
            std::chrono::milliseconds refresh_rate_{250}; // Default 250ms refresh rate
            bool show_cpu_utilization_{true};
            bool show_deadlines_{true};
            bool show_task_details_{true};
            bool show_progress_bars_{true};

            // Dashboard thread
            std::thread dashboard_thread_;
            std::atomic<bool> is_running_{false};
            std::mutex dashboard_mutex_;

            // Dashboard loop method
            void dashboardLoop();

            // Console utilities
            void setConsoleColor(ConsoleColor foreground, ConsoleColor background = ConsoleColor::BLACK);
            void resetConsoleColor();
            ConsoleColor getColorForTaskState(TaskState state);
            void clearConsole();
            void setCursorPosition(short x, short y);

            // Dashboard rendering methods
            void renderHeader();
            void renderTaskList();
            void renderTaskDetails();
            void renderCpuUtilization();
            std::string generateProgressBar(float percentage, int width, const std::string &fill_char = "=",
                                            const std::string &empty_char = " ");
        };

    } // namespace util
} // namespace edurtos
