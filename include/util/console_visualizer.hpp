#pragma once

#include "../kernel/task.hpp"
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <chrono>

namespace edurtos
{
    namespace util
    {

        class ConsoleVisualizer
        {
        public:
            enum class DisplayMode
            {
                SIMPLE,   // Basic task state display
                DETAILED, // Detailed task information
                TIMELINE, // Timeline view of task execution
                GRAPH     // ASCII graph of task metrics
            };

            ConsoleVisualizer();

            // Configure display options
            void setDisplayMode(DisplayMode mode);
            void setRefreshRate(std::chrono::milliseconds rate);
            void setShowPriorities(bool show);
            void setShowDeadlines(bool show);

            // Add tasks to visualize
            void addTask(TaskPtr task, char symbol = '\0');
            void removeTask(const std::string &task_name);

            // Generate visualization
            std::string generateTaskStateVisualization();
            std::string generateTaskTimelineVisualization(std::chrono::seconds duration);
            std::string generateTaskMetricsVisualization();

            // Display to console
            void display();

        private:
            DisplayMode mode_{DisplayMode::SIMPLE};
            std::chrono::milliseconds refresh_rate_{500};
            bool show_priorities_{true};
            bool show_deadlines_{true};
            std::map<TaskPtr, char> task_symbols_;
            std::chrono::steady_clock::time_point last_refresh_;

            // Timeline tracking
            struct TimelineEvent
            {
                std::chrono::steady_clock::time_point timestamp;
                TaskPtr task;
                TaskState previous_state;
                TaskState new_state;
            };
            std::vector<TimelineEvent> timeline_events_;

            // Utility functions
            char getDefaultSymbol(size_t index) const;
            std::string getTaskSymbol(TaskPtr task) const;
            std::string getTaskStateChar(TaskState state) const;
            std::string generateProgressBar(float percentage, int width = 20) const;
            void recordTaskStateChange(TaskPtr task, TaskState previous, TaskState current);
        };

    } // namespace util
} // namespace edurtos
