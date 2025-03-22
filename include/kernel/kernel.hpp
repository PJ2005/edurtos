#pragma once

#include "scheduler.hpp"
#include <memory>
#include <string>
#include <chrono>
#include <map>
#include <mutex>

namespace edurtos
{

    class Kernel
    {
    private:
        std::unique_ptr<Scheduler> scheduler_;
        std::map<std::string, TaskPtr> tasks_;
        std::mutex kernel_mutex_;
        static Kernel *instance_;

        // Visualization options
        bool auto_visualization_ = false;
        std::chrono::milliseconds visualization_interval_{1000};

    public:
        Kernel();
        ~Kernel();

        // Singleton access
        static Kernel &getInstance();

        // Kernel control
        void initialize();
        void start();
        void stop();

        // Task management
        TaskPtr createTask(const std::string &name,
                           std::function<void()> handler,
                           std::uint8_t priority = 128,
                           SchedulePolicy policy = SchedulePolicy::PREEMPTIVE,
                           std::chrono::milliseconds period = std::chrono::milliseconds(0),
                           std::chrono::milliseconds deadline = std::chrono::milliseconds(0),
                           bool recoverable = false);

        void removeTask(const std::string &name);
        TaskPtr getTask(const std::string &name);

        // Task control
        void suspendTask(const std::string &name);
        void resumeTask(const std::string &name);

        // Get scheduler instance
        Scheduler *getScheduler() const { return scheduler_.get(); }

        // Visualization
        void enableAutoVisualization(bool enable,
                                     std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
        void visualizeTaskStates();

    private:
        void visualizationLoop();
    };

} // namespace edurtos
