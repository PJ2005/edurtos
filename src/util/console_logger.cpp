#include "../../include/util/console_logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace edurtos
{
    namespace util
    {

        ConsoleLogger &ConsoleLogger::getInstance()
        {
            static ConsoleLogger instance;
            return instance;
        }

        void ConsoleLogger::init(const std::string &filename)
        {
            std::lock_guard<std::mutex> lock(log_mutex_);

            // Close any existing file
            if (log_file_.is_open())
            {
                log_file_.close();
            }

            // Open new log file
            log_file_.open(filename, std::ios::out | std::ios::trunc);

            if (!log_file_.is_open())
            {
                std::cerr << "Error: Could not open log file: " << filename << std::endl;
                return;
            }

            // Write header with timestamp
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);

            log_file_ << "==================================================" << std::endl;
            log_file_ << "EduRTOS Test Output Log" << std::endl;
            log_file_ << "Started at: " << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S") << std::endl;
            log_file_ << "==================================================" << std::endl
                      << std::endl;
            log_file_.flush();
        }

        void ConsoleLogger::close()
        {
            std::lock_guard<std::mutex> lock(log_mutex_);

            if (log_file_.is_open())
            {
                // Write footer with timestamp
                auto now = std::chrono::system_clock::now();
                auto now_time_t = std::chrono::system_clock::to_time_t(now);

                log_file_ << std::endl;
                log_file_ << "==================================================" << std::endl;
                log_file_ << "Log ended at: " << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S") << std::endl;
                log_file_ << "==================================================" << std::endl;

                log_file_.close();
            }
        }

        void ConsoleLogger::log(const std::string &message)
        {
            std::lock_guard<std::mutex> lock(log_mutex_);

            if (log_file_.is_open())
            {
                log_file_ << message << std::endl;
                log_file_.flush();
            }

            std::cout << message << std::endl;
        }

        ConsoleLogger::~ConsoleLogger()
        {
            close();
        }

    } // namespace util
} // namespace edurtos
