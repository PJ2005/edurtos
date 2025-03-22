#pragma once

#include "../kernel/scheduler.hpp"
#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <atomic>
#include <thread>

namespace edurtos
{
    namespace util
    {

        // Class to log scheduler decisions to CSV
        class SchedulerLogger
        {
        public:
            // Constructor takes a reference to the scheduler and a filename
            SchedulerLogger(Scheduler &scheduler, const std::string &filename = "scheduler_log.csv");
            ~SchedulerLogger();

            // Start/stop logging
            void start();
            void stop();

            // Set logging interval
            void setLoggingInterval(std::chrono::milliseconds interval);

            // Log an event with custom message
            void logEvent(const std::string &event_type, const std::string &message);

            // Flush log to disk
            void flush();

        private:
            // Reference to scheduler
            Scheduler &scheduler_;

            // File handling
            std::string filename_;
            std::ofstream log_file_;
            std::mutex file_mutex_;

            // Logging state
            std::atomic<bool> is_running_{false};
            std::chrono::milliseconds logging_interval_{100}; // Default 100ms
            std::thread logging_thread_;

            // Logging methods
            void loggingLoop();
            void writeHeader();
            void logSchedulerState();
            void logTaskState(const TaskPtr &task, const std::string &event);
            std::string getCurrentTimestamp() const;
        };

    } // namespace util
} // namespace edurtos
