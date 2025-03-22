#pragma once

#include "task.hpp"
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>

namespace edurtos
{
    class Scheduler
    {
    public:
        struct TaskPriorityCompare
        {
            bool operator()(const TaskPtr &a, const TaskPtr &b) const
            {
                return a->getDynamicPriority() < b->getDynamicPriority();
            }
        };

        enum class PreemptionMode
        {
            NONE,       // No preemption (fully cooperative)
            TIME_SLICE, // Preemption based on time slices
            PRIORITY,   // Preemption based on priority
            HYBRID      // Both time slice and priority based preemption
        };

    private:
        std::vector<TaskPtr> all_tasks_;
        std::priority_queue<TaskPtr, std::vector<TaskPtr>, TaskPriorityCompare> ready_queue_;
        TaskPtr current_task_;
        std::atomic<bool> is_running_{false};
        std::thread scheduler_thread_;
        std::thread deadline_monitor_thread_;
        std::mutex scheduler_mutex_;
        std::condition_variable scheduler_cv_;
        std::chrono::milliseconds time_slice_{50}; // Default time slice of 50ms
        std::chrono::steady_clock::time_point last_schedule_time_;
        PreemptionMode preemption_mode_{PreemptionMode::HYBRID};
        std::atomic<bool> force_reschedule_{false};

        // For visualization
        std::map<TaskPtr, char> task_symbols_;
        std::atomic<float> cpu_utilization_{0.0f};
        std::chrono::steady_clock::time_point idle_start_time_;
        std::chrono::microseconds total_run_time_{0};
        std::chrono::microseconds total_idle_time_{0};

        // For recovery
        std::atomic<size_t> recovery_attempts_{0};
        static constexpr size_t MAX_RECOVERY_ATTEMPTS = 3;

    public:
        Scheduler(std::chrono::milliseconds time_slice = std::chrono::milliseconds(50));
        ~Scheduler();

        // Task management
        void addTask(TaskPtr task);
        void removeTask(const std::string &name);
        TaskPtr findTask(const std::string &name);

        // Get the currently running task
        TaskPtr getCurrentTask() const { return current_task_; }

        // Get all tasks
        const std::vector<TaskPtr> &getAllTasks() const { return all_tasks_; }

        // Scheduler control
        void start();
        void stop();
        void yield(); // Cooperative yield
        void setPreemptionMode(PreemptionMode mode);
        PreemptionMode getPreemptionMode() const { return preemption_mode_; }
        void setTimeSlice(std::chrono::milliseconds time_slice);
        std::chrono::milliseconds getTimeSlice() const { return time_slice_; }

        // Adaptive priority
        void adjustPriorities();

        // Performance metrics
        float getCpuUtilization() const { return cpu_utilization_; }
        void updateCpuUtilization();

        // Visualization
        void printTaskStates();
        std::string getTaskStateVisualization();

        // Recovery
        bool attemptTaskRecovery(TaskPtr task);

    private:
        void schedulerLoop();
        void deadlineMonitorLoop();
        TaskPtr selectNextTask();
        void updateTaskStatistics();
        void checkDeadlines();
        bool shouldPreempt(TaskPtr new_task) const;
        char getSymbolForTaskState(TaskState state);
        void enterIdleState();
        void exitIdleState();
    };

} // namespace edurtos
