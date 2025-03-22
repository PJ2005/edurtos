#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>

namespace edurtos
{
    namespace util
    {

        class ConsoleLogger
        {
        public:
            // Get singleton instance
            static ConsoleLogger &getInstance();

            // Initialize with output file
            void init(const std::string &filename = "console_output.txt");

            // Close and finalize log
            void close();

            // Log a message to both console and file
            void log(const std::string &message);

            // Stream operators for convenient logging
            template <typename T>
            ConsoleLogger &operator<<(const T &data)
            {
                std::lock_guard<std::mutex> lock(log_mutex_);
                if (log_file_.is_open())
                {
                    log_file_ << data;
                    log_file_.flush();
                }
                std::cout << data;
                return *this;
            }

            // Handle endl and other manipulators
            ConsoleLogger &operator<<(std::ostream &(*manip)(std::ostream &))
            {
                std::lock_guard<std::mutex> lock(log_mutex_);
                if (log_file_.is_open())
                {
                    manip(log_file_);
                    log_file_.flush();
                }
                manip(std::cout);
                return *this;
            }

        private:
            // Private constructor for singleton
            ConsoleLogger() = default;
            ~ConsoleLogger();

            // Prevent copying
            ConsoleLogger(const ConsoleLogger &) = delete;
            ConsoleLogger &operator=(const ConsoleLogger &) = delete;

            // Log file
            std::ofstream log_file_;
            std::mutex log_mutex_;
        };

        // Convenience function for logging
        inline void log(const std::string &message)
        {
            ConsoleLogger::getInstance().log(message);
        }

// Convenience macro for quick logging
#define LOG(x) ConsoleLogger::getInstance() << x << std::endl

    } // namespace util
} // namespace edurtos
