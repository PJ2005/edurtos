#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <atomic>

namespace edurtos
{

    enum class TaskState
    {
        READY,     // Task ready to be executed
        RUNNING,   // Task currently executing
        BLOCKED,   // Task waiting for resource/event
        SUSPENDED, // Task suspended by user
        TERMINATED // Task completed execution
    };

    enum class SchedulePolicy
    {
        PREEMPTIVE, // Can be interrupted by higher priority tasks
        COOPERATIVE // Must yield voluntarily
    };

    struct TaskStatistics
    {
        std::size_t execution_count = 0;
        std::size_t deadline_misses = 0;
        std::chrono::steady_clock::time_point last_execution{};
        std::chrono::microseconds total_execution_time{0};
        std::chrono::microseconds average_execution_time{0};
        std::chrono::milliseconds deadline_counter{0}; // New deadline counter
    };

    template <typename T = void>
    class TaskBase;

    using Task = TaskBase<void>; // Default task specialization

    template <typename T>
    class TaskBase
    {
    private:
        std::string name_;
        std::function<void()> handler_;
        std::atomic<TaskState> state_{TaskState::READY};
        SchedulePolicy policy_;
        std::uint8_t base_priority_;
        std::atomic<std::uint8_t> dynamic_priority_; // 1-99 scale
        std::chrono::milliseconds period_{0};
        std::chrono::milliseconds deadline_{0};
        TaskStatistics statistics_{};
        std::size_t stack_size_;
        bool recoverable_;

    public:
        TaskBase(std::string name,
                 std::function<void()> handler,
                 std::uint8_t priority,
                 SchedulePolicy policy = SchedulePolicy::PREEMPTIVE,
                 std::chrono::milliseconds period = std::chrono::milliseconds(0),
                 std::chrono::milliseconds deadline = std::chrono::milliseconds(0),
                 std::size_t stack_size = 4096,
                 bool recoverable = false);

        // Core task operations
        void execute();
        void suspend();
        void resume();
        void terminate();

        // Getters
        const std::string &getName() const { return name_; }
        TaskState getState() const { return state_; }
        SchedulePolicy getPolicy() const { return policy_; }
        std::uint8_t getBasePriority() const { return base_priority_; }
        std::uint8_t getDynamicPriority() const { return dynamic_priority_; }
        std::chrono::milliseconds getPeriod() const { return period_; }
        std::chrono::milliseconds getDeadline() const { return deadline_; }
        const TaskStatistics &getStatistics() const { return statistics_; }
        bool isRecoverable() const { return recoverable_; }

        // State update methods
        void recordDeadlineMiss();
        void updatePriority();
        void resetStatistics();
        void updateDeadlineCounter(std::chrono::milliseconds elapsed);
        bool isDeadlineApproaching() const;

        // For scheduler use only
        void setState(TaskState state) { state_ = state; }
        void updateStatistics(std::chrono::microseconds execution_time);
    };

    using TaskPtr = std::shared_ptr<Task>;

} // namespace edurtos
