#include "../../include/util/console_visualizer.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace edurtos
{
    namespace util
    {

        ConsoleVisualizer::ConsoleVisualizer()
            : last_refresh_(std::chrono::steady_clock::now())
        {
        }

        void ConsoleVisualizer::setDisplayMode(DisplayMode mode)
        {
            mode_ = mode;
        }

        void ConsoleVisualizer::setRefreshRate(std::chrono::milliseconds rate)
        {
            refresh_rate_ = rate;
        }

        void ConsoleVisualizer::setShowPriorities(bool show)
        {
            show_priorities_ = show;
        }

        void ConsoleVisualizer::setShowDeadlines(bool show)
        {
            show_deadlines_ = show;
        }

        void ConsoleVisualizer::addTask(TaskPtr task, char symbol)
        {
            if (symbol == '\0')
            {
                // Assign default symbol
                symbol = getDefaultSymbol(task_symbols_.size());
            }
            task_symbols_[task] = symbol;
        }

        void ConsoleVisualizer::removeTask(const std::string &task_name)
        {
            for (auto it = task_symbols_.begin(); it != task_symbols_.end();)
            {
                if (it->first->getName() == task_name)
                {
                    it = task_symbols_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        char ConsoleVisualizer::getDefaultSymbol(size_t index) const
        {
            static const std::string symbols = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            if (index < symbols.size())
            {
                return symbols[index];
            }
            return '#'; // Default symbol if we run out
        }

        std::string ConsoleVisualizer::getTaskSymbol(TaskPtr task) const
        {
            auto it = task_symbols_.find(task);
            if (it != task_symbols_.end())
            {
                return std::string(1, it->second);
            }
            return "?";
        }

        std::string ConsoleVisualizer::getTaskStateChar(TaskState state) const
        {
            switch (state)
            {
            case TaskState::READY:
                return ".";
            case TaskState::RUNNING:
                return "R";
            case TaskState::BLOCKED:
                return "B";
            case TaskState::SUSPENDED:
                return "S";
            case TaskState::TERMINATED:
                return "T";
            default:
                return "?";
            }
        }

        std::string ConsoleVisualizer::generateProgressBar(float percentage, int width) const
        {
            int fill_width = static_cast<int>(width * percentage / 100.0f);
            std::stringstream ss;
            ss << "[";
            for (int i = 0; i < width; ++i)
            {
                if (i < fill_width)
                {
                    ss << "=";
                }
                else if (i == fill_width)
                {
                    ss << ">";
                }
                else
                {
                    ss << " ";
                }
            }
            ss << "] " << std::fixed << std::setprecision(1) << percentage << "%";
            return ss.str();
        }

        void ConsoleVisualizer::recordTaskStateChange(TaskPtr task, TaskState previous, TaskState current)
        {
            TimelineEvent event;
            event.timestamp = std::chrono::steady_clock::now();
            event.task = task;
            event.previous_state = previous;
            event.new_state = current;
            timeline_events_.push_back(event);

            // Limit timeline size to prevent excessive memory usage
            if (timeline_events_.size() > 1000)
            {
                timeline_events_.erase(timeline_events_.begin());
            }
        }

        std::string ConsoleVisualizer::generateTaskStateVisualization()
        {
            std::stringstream ss;

            // Header row with task symbols
            ss << "Time | ";
            for (const auto &[task, symbol] : task_symbols_)
            {
                ss << symbol << " ";
            }
            ss << "| Tasks\n";

            // Separator
            ss << "-----|-";
            for (size_t i = 0; i < task_symbols_.size(); i++)
            {
                ss << "--";
            }
            ss << "|---------\n";

            // Current state
            ss << "now  | ";
            for (const auto &[task, symbol] : task_symbols_)
            {
                ss << getTaskStateChar(task->getState()) << " ";
            }
            ss << "| ";

            // Print task names and additional information
            bool first = true;
            for (const auto &[task, symbol] : task_symbols_)
            {
                if (!first)
                    ss << ", ";
                first = false;
                ss << symbol << ":" << task->getName();

                if (show_priorities_)
                {
                    ss << "(" << static_cast<int>(task->getDynamicPriority()) << ")";
                }

                if (show_deadlines_ && task->getDeadline().count() > 0)
                {
                    float deadline_percentage = 100.0f *
                                                task->getStatistics().deadline_counter.count() /
                                                task->getDeadline().count();

                    ss << " " << std::fixed << std::setprecision(1) << deadline_percentage << "%";

                    if (task->getStatistics().deadline_misses > 0)
                    {
                        ss << " [" << task->getStatistics().deadline_misses << " misses]";
                    }
                }
            }

            return ss.str();
        }

        std::string ConsoleVisualizer::generateTaskTimelineVisualization(std::chrono::seconds duration)
        {
            // Filter events within the specified duration
            auto now = std::chrono::steady_clock::now();
            auto cutoff = now - duration;

            std::vector<TimelineEvent> recent_events;
            for (const auto &event : timeline_events_)
            {
                if (event.timestamp >= cutoff)
                {
                    recent_events.push_back(event);
                }
            }

            // Create timeline visualization
            std::stringstream ss;
            ss << "Task Timeline (last " << duration.count() << " seconds):\n";

            // Create a timeline for each task
            for (const auto &[task, symbol] : task_symbols_)
            {
                ss << symbol << ":" << task->getName() << " ";

                // Create timeline segments
                const int TIMELINE_WIDTH = 60;
                std::vector<char> timeline(TIMELINE_WIDTH, ' ');

                for (const auto &event : recent_events)
                {
                    if (event.task == task)
                    {
                        // Calculate position in timeline
                        float position_percentage = static_cast<float>(
                                                        std::chrono::duration_cast<std::chrono::milliseconds>(event.timestamp - cutoff).count()) /
                                                    std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

                        int position = static_cast<int>(position_percentage * TIMELINE_WIDTH);
                        if (position >= 0 && position < TIMELINE_WIDTH)
                        {
                            timeline[position] = getTaskStateChar(event.new_state)[0];
                        }
                    }
                }

                // Print the timeline
                ss << "[";
                for (char c : timeline)
                {
                    ss << c;
                }
                ss << "]\n";
            }

            return ss.str();
        }

        std::string ConsoleVisualizer::generateTaskMetricsVisualization()
        {
            std::stringstream ss;
            ss << "Task Metrics:\n";
            ss << "+-" << std::string(20, '-') << "-+-" << std::string(8, '-') << "-+-"
               << std::string(10, '-') << "-+-" << std::string(10, '-') << "-+-"
               << std::string(12, '-') << "-+\n";

            ss << "| " << std::left << std::setw(20) << "Task Name" << " | "
               << std::setw(8) << "Priority" << " | "
               << std::setw(10) << "Exec Count" << " | "
               << std::setw(10) << "Deadline%" << " | "
               << std::setw(12) << "Avg Exec (ms)" << " |\n";

            ss << "+-" << std::string(20, '-') << "-+-" << std::string(8, '-') << "-+-"
               << std::string(10, '-') << "-+-" << std::string(10, '-') << "-+-"
               << std::string(12, '-') << "-+\n";

            for (const auto &[task, symbol] : task_symbols_)
            {
                float deadline_percentage = 0.0f;
                if (task->getDeadline().count() > 0)
                {
                    deadline_percentage = 100.0f *
                                          task->getStatistics().deadline_counter.count() /
                                          task->getDeadline().count();
                }

                ss << "| " << std::left << std::setw(20) << task->getName() << " | "
                   << std::right << std::setw(8) << static_cast<int>(task->getDynamicPriority()) << " | "
                   << std::setw(10) << task->getStatistics().execution_count << " | ";

                if (task->getDeadline().count() > 0)
                {
                    ss << std::setw(10) << std::fixed << std::setprecision(1) << deadline_percentage << "% | ";
                }
                else
                {
                    ss << std::setw(10) << "N/A | ";
                }

                ss << std::setw(12) << std::fixed << std::setprecision(2)
                   << task->getStatistics().average_execution_time.count() / 1000.0 << " |\n";
            }

            ss << "+-" << std::string(20, '-') << "-+-" << std::string(8, '-') << "-+-"
               << std::string(10, '-') << "-+-" << std::string(10, '-') << "-+-"
               << std::string(12, '-') << "-+\n";

            return ss.str();
        }

        void ConsoleVisualizer::display()
        {
            auto now = std::chrono::steady_clock::now();
            if (now - last_refresh_ < refresh_rate_)
            {
                return;
            }
            last_refresh_ = now;

// Clear screen (cross-platform)
#ifdef _WIN32
            system("cls");
#else
            std::cout << "\033[2J\033[1;1H";
#endif

            switch (mode_)
            {
            case DisplayMode::SIMPLE:
                std::cout << generateTaskStateVisualization() << std::endl;
                break;

            case DisplayMode::DETAILED:
                std::cout << generateTaskStateVisualization() << "\n\n"
                          << generateTaskMetricsVisualization() << std::endl;
                break;

            case DisplayMode::TIMELINE:
                std::cout << generateTaskStateVisualization() << "\n\n"
                          << generateTaskTimelineVisualization(std::chrono::seconds(10)) << std::endl;
                break;

            case DisplayMode::GRAPH:
                std::cout << generateTaskStateVisualization() << "\n\n"
                          << "Task Priority Chart:\n";

                // Display ASCII bar chart of task priorities
                for (const auto &[task, symbol] : task_symbols_)
                {
                    float priority_percentage = task->getDynamicPriority() * 100.0f / 99.0f;
                    std::cout << std::left << std::setw(15) << task->getName() << " "
                              << generateProgressBar(priority_percentage, 30) << "\n";
                }
                break;
            }
        }

    } // namespace util
} // namespace edurtos
