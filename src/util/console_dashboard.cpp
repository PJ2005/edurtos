#include "../../include/util/console_dashboard.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace edurtos
{
    namespace util
    {

        ConsoleDashboard::ConsoleDashboard(Scheduler &scheduler)
            : scheduler_(scheduler)
        {
        }

        ConsoleDashboard::~ConsoleDashboard()
        {
            stop();
        }

        void ConsoleDashboard::start()
        {
            if (!is_running_.exchange(true))
            {
                dashboard_thread_ = std::thread(&ConsoleDashboard::dashboardLoop, this);
            }
        }

        void ConsoleDashboard::stop()
        {
            if (is_running_.exchange(false))
            {
                if (dashboard_thread_.joinable())
                {
                    dashboard_thread_.join();
                }
            }
        }

        void ConsoleDashboard::setRefreshRate(std::chrono::milliseconds rate)
        {
            refresh_rate_ = rate;
        }

        void ConsoleDashboard::showCpuUtilization(bool show)
        {
            show_cpu_utilization_ = show;
        }

        void ConsoleDashboard::showDeadlines(bool show)
        {
            show_deadlines_ = show;
        }

        void ConsoleDashboard::showTaskDetails(bool show)
        {
            show_task_details_ = show;
        }

        void ConsoleDashboard::showProgressBars(bool show)
        {
            show_progress_bars_ = show;
        }

        void ConsoleDashboard::refresh()
        {
            std::lock_guard<std::mutex> lock(dashboard_mutex_);

            clearConsole();
            renderHeader();
            renderTaskList();

            if (show_task_details_)
            {
                renderTaskDetails();
            }

            if (show_cpu_utilization_)
            {
                renderCpuUtilization();
            }
        }

        void ConsoleDashboard::dashboardLoop()
        {
            while (is_running_)
            {
                refresh();
                std::this_thread::sleep_for(refresh_rate_);
            }
        }

        void ConsoleDashboard::setConsoleColor(ConsoleColor foreground, ConsoleColor background)
        {
#ifdef _WIN32
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hConsole != INVALID_HANDLE_VALUE)
            {
                SetConsoleTextAttribute(hConsole,
                                        static_cast<WORD>(foreground) | (static_cast<WORD>(background) << 4));
            }
#else
            // ANSI color codes for non-Windows systems
            int fg = static_cast<int>(foreground);
            int bg = static_cast<int>(background);

            std::cout << "\033[" << (fg < 8 ? 30 + fg : 90 + fg - 8) << ";"
                      << (bg < 8 ? 40 + bg : 100 + bg - 8) << "m";
#endif
        }

        void ConsoleDashboard::resetConsoleColor()
        {
#ifdef _WIN32
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hConsole != INVALID_HANDLE_VALUE)
            {
                SetConsoleTextAttribute(hConsole,
                                        static_cast<WORD>(ConsoleColor::LIGHT_GRAY) | (static_cast<WORD>(ConsoleColor::BLACK) << 4));
            }
#else
            std::cout << "\033[0m"; // Reset to default
#endif
        }

        ConsoleColor ConsoleDashboard::getColorForTaskState(TaskState state)
        {
            switch (state)
            {
            case TaskState::RUNNING:
                return ConsoleColor::RED; // Running = Red
            case TaskState::READY:
                return ConsoleColor::YELLOW; // Ready = Yellow
            case TaskState::BLOCKED:
                return ConsoleColor::BLUE; // Blocked = Blue
            case TaskState::SUSPENDED:
                return ConsoleColor::DARK_GRAY; // Suspended = Dark Gray
            case TaskState::TERMINATED:
                return ConsoleColor::MAGENTA; // Terminated = Magenta
            default:
                return ConsoleColor::LIGHT_GRAY; // Default = Light Gray
            }
        }

        void ConsoleDashboard::clearConsole()
        {
#ifdef _WIN32
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hConsole != INVALID_HANDLE_VALUE)
            {
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                COORD topLeft = {0, 0};

                // Get console info
                if (GetConsoleScreenBufferInfo(hConsole, &csbi))
                {
                    DWORD length = csbi.dwSize.X * csbi.dwSize.Y;
                    DWORD written;

                    // Fill with spaces
                    FillConsoleOutputCharacter(hConsole, ' ', length, topLeft, &written);
                    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, length, topLeft, &written);

                    // Move cursor to top-left
                    SetConsoleCursorPosition(hConsole, topLeft);
                }
            }
#else
            // ANSI escape code to clear screen and move cursor to home position
            std::cout << "\033[2J\033[H";
#endif
        }

        void ConsoleDashboard::setCursorPosition(short x, short y)
        {
#ifdef _WIN32
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hConsole != INVALID_HANDLE_VALUE)
            {
                COORD pos = {x, y};
                SetConsoleCursorPosition(hConsole, pos);
            }
#else
            // ANSI escape code to set cursor position
            std::cout << "\033[" << (y + 1) << ";" << (x + 1) << "H";
#endif
        }

        void ConsoleDashboard::renderHeader()
        {
            setConsoleColor(ConsoleColor::WHITE, ConsoleColor::BLUE);
            std::cout << " EduRTOS Dashboard";

            // Get console width
            int width = 80; // Default
#ifdef _WIN32
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hConsole != INVALID_HANDLE_VALUE)
            {
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                if (GetConsoleScreenBufferInfo(hConsole, &csbi))
                {
                    width = csbi.dwSize.X;
                }
            }
#endif

            // Pad the header to fill the width
            for (int i = 16; i < width; i++)
            {
                std::cout << " ";
            }

            resetConsoleColor();
            std::cout << "\n\n";
        }

        void ConsoleDashboard::renderTaskList()
        {
            auto &tasks = scheduler_.getAllTasks();
            auto current_task = scheduler_.getCurrentTask();

            // Column headers
            setConsoleColor(ConsoleColor::WHITE);
            std::cout << std::left
                      << std::setw(20) << "Task Name"
                      << std::setw(10) << "Priority"
                      << std::setw(10) << "State"
                      << std::setw(10) << "Deadline";

            if (show_deadlines_)
            {
                std::cout << std::setw(15) << "Deadline %";
            }

            std::cout << "\n";
            resetConsoleColor();

            // Separator line
            std::cout << std::string(60, '-') << "\n";

            // Task list
            for (const auto &task : tasks)
            {
                // Set color based on task state
                setConsoleColor(getColorForTaskState(task->getState()));

                // Highlight current task with a different background
                if (task == current_task)
                {
                    setConsoleColor(ConsoleColor::BLACK, ConsoleColor::LIGHT_GRAY);
                }

                // Print task info
                std::cout << std::left
                          << std::setw(20) << task->getName()
                          << std::setw(10) << static_cast<int>(task->getDynamicPriority())
                          << std::setw(10);

                // Print state name
                switch (task->getState())
                {
                case TaskState::RUNNING:
                    std::cout << "RUNNING";
                    break;
                case TaskState::READY:
                    std::cout << "READY";
                    break;
                case TaskState::BLOCKED:
                    std::cout << "BLOCKED";
                    break;
                case TaskState::SUSPENDED:
                    std::cout << "SUSPENDED";
                    break;
                case TaskState::TERMINATED:
                    std::cout << "TERMINATED";
                    break;
                default:
                    std::cout << "UNKNOWN";
                    break;
                }

                // Print deadline info
                std::cout << std::setw(10) << task->getDeadline().count();

                // Show deadline percentage if enabled
                if (show_deadlines_ && task->getDeadline().count() > 0)
                {
                    float deadline_percentage = 100.0f *
                                                task->getStatistics().deadline_counter.count() /
                                                task->getDeadline().count();

                    std::cout << std::setw(5) << std::fixed << std::setprecision(1) << deadline_percentage << "% ";

                    if (show_progress_bars_)
                    {
                        std::cout << generateProgressBar(deadline_percentage, 10);
                    }
                }

                std::cout << "\n";
                resetConsoleColor();
            }

            std::cout << "\n";
        }

        void ConsoleDashboard::renderTaskDetails()
        {
            auto current_task = scheduler_.getCurrentTask();

            std::cout << "Task Details:\n";
            std::cout << "-----------------\n";

            if (current_task)
            {
                setConsoleColor(ConsoleColor::GREEN);
                std::cout << "Current Task: " << current_task->getName() << "\n";
                resetConsoleColor();

                auto &stats = current_task->getStatistics();
                std::cout << "Executions: " << stats.execution_count << "\n";
                std::cout << "Deadline Misses: " << stats.deadline_misses << "\n";
                std::cout << "Average Execution Time: " << std::fixed << std::setprecision(2) << (stats.average_execution_time.count() / 1000.0) << " ms\n";
            }
            else
            {
                setConsoleColor(ConsoleColor::DARK_GRAY);
                std::cout << "No task currently running (idle)\n";
                resetConsoleColor();
            }

            std::cout << "\n";
        }

        void ConsoleDashboard::renderCpuUtilization()
        {
            float utilization = scheduler_.getCpuUtilization();

            std::cout << "CPU Utilization: " << std::fixed << std::setprecision(1) << utilization << "%\n";

            if (show_progress_bars_)
            {
                // Color-coded progress bar
                if (utilization < 50.0f)
                {
                    setConsoleColor(ConsoleColor::GREEN);
                }
                else if (utilization < 80.0f)
                {
                    setConsoleColor(ConsoleColor::YELLOW);
                }
                else
                {
                    setConsoleColor(ConsoleColor::RED);
                }

                std::cout << generateProgressBar(utilization, 50, "█", "░") << "\n";
                resetConsoleColor();
            }

            std::cout << "\n";
        }

        std::string ConsoleDashboard::generateProgressBar(float percentage, int width,
                                                          const std::string &fill_char,
                                                          const std::string &empty_char)
        {
            // Limit percentage to range [0, 100]
            percentage = std::max(0.0f, std::min(100.0f, percentage));

            // Calculate filled width
            int filled_width = static_cast<int>(width * percentage / 100.0f);

            // Build progress bar string
            std::stringstream ss;
            ss << "[";

            for (int i = 0; i < width; i++)
            {
                if (i < filled_width)
                {
                    ss << fill_char;
                }
                else
                {
                    ss << empty_char;
                }
            }

            ss << "] ";

            return ss.str();
        }

    } // namespace util
} // namespace edurtos
