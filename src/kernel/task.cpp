#include "../../include/kernel/task.hpp"

namespace edurtos
{
    template <typename T>
    TaskBase<T>::TaskBase(std::string name,
                          std::function<void()> handler,
                          std::uint8_t priority,
                          SchedulePolicy policy,
                          std::chrono::milliseconds period,
                          std::chrono::milliseconds deadline,
                          std::size_t stack_size,
                          bool recoverable)
        : name_(std::move(name)),
          handler_(std::move(handler)),
          policy_(policy),
          base_priority_(std::min<std::uint8_t>(priority, 99)), // Ensure priority is in 1-99 range
          dynamic_priority_(std::min<std::uint8_t>(priority, 99)),
          period_(period),
          deadline_(deadline.count() > 0 ? deadline : period),
          stack_size_(stack_size),
          recoverable_(recoverable)
    {
    }

    template <typename T>
    void TaskBase<T>::execute()
    {
        state_ = TaskState::RUNNING;
        statistics_.last_execution = std::chrono::steady_clock::now();
        statistics_.execution_count++;
        // Reset deadline counter when task starts execution
        statistics_.deadline_counter = std::chrono::milliseconds(0);

        try
        {
            handler_();
        }
        catch (...)
        {
            // Task execution failed
            if (!recoverable_)
            {
                state_ = TaskState::TERMINATED;
            }
            else
            {
                state_ = TaskState::READY;
            }
            return;
        }

        state_ = TaskState::READY;
    }

    template <typename T>
    void TaskBase<T>::suspend()
    {
        if (state_ != TaskState::TERMINATED)
        {
            state_ = TaskState::SUSPENDED;
        }
    }

    template <typename T>
    void TaskBase<T>::resume()
    {
        if (state_ == TaskState::SUSPENDED)
        {
            state_ = TaskState::READY;
        }
    }

    template <typename T>
    void TaskBase<T>::terminate()
    {
        state_ = TaskState::TERMINATED;
    }

    template <typename T>
    void TaskBase<T>::recordDeadlineMiss()
    {
        statistics_.deadline_misses++;
        updatePriority();
    }

    template <typename T>
    void TaskBase<T>::updatePriority()
    {
        // Adaptive priority algorithm - increase priority by 5% after missed deadline
        if (statistics_.deadline_misses > 0)
        {
            // Calculate 5% increase per missed deadline, capped at 99
            float priority_boost = base_priority_ * 0.05f * statistics_.deadline_misses;
            dynamic_priority_ = static_cast<std::uint8_t>(
                std::min(99, static_cast<int>(base_priority_ + priority_boost)));
        }
        else
        {
            dynamic_priority_ = base_priority_;
        }
    }

    template <typename T>
    void TaskBase<T>::updateStatistics(std::chrono::microseconds execution_time)
    {
        statistics_.total_execution_time += execution_time;
        if (statistics_.execution_count > 0)
        {
            statistics_.average_execution_time = std::chrono::microseconds(
                statistics_.total_execution_time.count() / statistics_.execution_count);
        }
    }

    template <typename T>
    void TaskBase<T>::updateDeadlineCounter(std::chrono::milliseconds elapsed)
    {
        if (deadline_.count() > 0)
        {
            statistics_.deadline_counter += elapsed;

            // Check if deadline is exceeded
            if (statistics_.deadline_counter > deadline_)
            {
                recordDeadlineMiss();
                // Reset deadline counter
                statistics_.deadline_counter = std::chrono::milliseconds(0);
            }
        }
    }

    template <typename T>
    bool TaskBase<T>::isDeadlineApproaching() const
    {
        if (deadline_.count() == 0)
            return false;

        // Consider deadline approaching if 80% of deadline time has passed
        return statistics_.deadline_counter > (deadline_ * 4 / 5);
    }

    template <typename T>
    void TaskBase<T>::resetStatistics()
    {
        statistics_.execution_count = 0;
        statistics_.deadline_misses = 0;
        statistics_.total_execution_time = std::chrono::microseconds(0);
        statistics_.average_execution_time = std::chrono::microseconds(0);
        statistics_.deadline_counter = std::chrono::milliseconds(0);
        dynamic_priority_ = base_priority_;
    }

    // Explicitly instantiate the TaskBase<void> specialization
    // Note: This should only be present in the .cpp file, not in the header
    template class TaskBase<void>;

} // namespace edurtos
