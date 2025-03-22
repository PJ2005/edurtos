#include "../../include/util/scheduler_logger.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>

namespace edurtos
{
    namespace util
    {

        SchedulerLogger::SchedulerLogger(Scheduler &scheduler, const std::string &filename)
            : scheduler_(scheduler), filename_(filename)
        {
            try
            {
                log_file_.open(filename_, std::ios::out | std::ios::trunc);
                if (!log_file_.is_open())
                {
                    std::cerr << "Error: Could not open log file: " << filename_ << std::endl;
                }
                else
                {
                    writeHeader();
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error initializing scheduler logger: " << e.what() << std::endl;
            }
        }

        SchedulerLogger::~SchedulerLogger()
        {
            stop();
            if (log_file_.is_open())
            {
                log_file_.close();
            }
        }

        void SchedulerLogger::start()
        {
            if (!is_running_.exchange(true))
            {
                logging_thread_ = std::thread(&SchedulerLogger::loggingLoop, this);
            }
        }

        void SchedulerLogger::stop()
        {
            if (is_running_.exchange(false))
            {
                if (logging_thread_.joinable())
                {
                    logging_thread_.join();
                }
                flush();
            }
        }

        void SchedulerLogger::setLoggingInterval(std::chrono::milliseconds interval)
        {
            logging_interval_ = interval;
        }

        void SchedulerLogger::writeHeader()
        {
            std::lock_guard<std::mutex> lock(file_mutex_);
            log_file_ << "Timestamp,EventType,TaskName,TaskState,Priority,DeadlineMs,DeadlinePercent,ExecutionCount,MissCount,AvgExecTimeMs,CPUUtilization" << std::endl;
            log_file_.flush();
        }

        std::string SchedulerLogger::getCurrentTimestamp() const
        {
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now.time_since_epoch()) %
                          1000;

            std::stringstream ss;
            ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S")
               << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
            return ss.str();
        }

        void SchedulerLogger::logEvent(const std::string &event_type, const std::string &message)
        {
            std::lock_guard<std::mutex> lock(file_mutex_);

            if (!log_file_.is_open())
                return;

            log_file_ << getCurrentTimestamp() << ","
                      << event_type << ","
                      << message << ",,,,,,,,," << std::endl;
        }

        void SchedulerLogger::logSchedulerState()
        {
            auto &tasks = scheduler_.getAllTasks();
            auto current_task = scheduler_.getCurrentTask();
            float cpu_utilization = scheduler_.getCpuUtilization();

            // Log current state for each task
            for (const auto &task : tasks)
            {
                logTaskState(task, task == current_task ? "RUNNING" : "STATE_UPDATE");
            }

            // Log CPU utilization
            std::lock_guard<std::mutex> lock(file_mutex_);
            log_file_ << getCurrentTimestamp() << ","
                      << "CPU_UTILIZATION,,,,,,,,,,"
                      << std::fixed << std::setprecision(2) << cpu_utilization
                      << std::endl;
        }

        void SchedulerLogger::logTaskState(const TaskPtr &task, const std::string &event)
        {
            std::string task_state;
            switch (task->getState())
            {
            case TaskState::READY:
                task_state = "READY";
                break;
            case TaskState::RUNNING:
                task_state = "RUNNING";
                break;
            case TaskState::BLOCKED:
                task_state = "BLOCKED";
                break;
            case TaskState::SUSPENDED:
                task_state = "SUSPENDED";
                break;
            case TaskState::TERMINATED:
                task_state = "TERMINATED";
                break;
            default:
                task_state = "UNKNOWN";
                break;
            }

            float deadline_percent = 0.0f;
            if (task->getDeadline().count() > 0)
            {
                deadline_percent = 100.0f *
                                   task->getStatistics().deadline_counter.count() /
                                   task->getDeadline().count();
            }

            float avg_exec_ms = task->getStatistics().average_execution_time.count() / 1000.0f;

            std::lock_guard<std::mutex> lock(file_mutex_);
            log_file_ << getCurrentTimestamp() << ","
                      << event << ","
                      << task->getName() << ","
                      << task_state << ","
                      << static_cast<int>(task->getDynamicPriority()) << ","
                      << task->getDeadline().count() << ","
                      << std::fixed << std::setprecision(2) << deadline_percent << ","
                      << task->getStatistics().execution_count << ","
                      << task->getStatistics().deadline_misses << ","
                      << std::fixed << std::setprecision(3) << avg_exec_ms << ","
                      << std::endl;
        }

        void SchedulerLogger::loggingLoop()
        {
            while (is_running_)
            {
                logSchedulerState();
                std::this_thread::sleep_for(logging_interval_);
            }
        }

        void SchedulerLogger::flush()
        {
            std::lock_guard<std::mutex> lock(file_mutex_);
            if (log_file_.is_open())
            {
                log_file_.flush();
            }
        }

    } // namespace util
} // namespace edurtos
