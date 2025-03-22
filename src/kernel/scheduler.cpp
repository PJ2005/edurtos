#include "../../include/kernel/scheduler.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace edurtos
{
    Scheduler::Scheduler(std::chrono::milliseconds time_slice)
        : time_slice_(time_slice)
    {
        idle_start_time_ = std::chrono::steady_clock::now();
    }

    Scheduler::~Scheduler()
    {
        stop();
    }

    void Scheduler::addTask(TaskPtr task)
    {
        std::lock_guard<std::mutex> lock(scheduler_mutex_);
        all_tasks_.push_back(task);

        if (task->getState() == TaskState::READY)
        {
            ready_queue_.push(task);
        }

        // Assign a symbol for visualization
        static const std::string symbols = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        if (all_tasks_.size() <= symbols.size())
        {
            task_symbols_[task] = symbols[all_tasks_.size() - 1];
        }
        else
        {
            task_symbols_[task] = '#'; // Default symbol if we run out
        }
    }

    void Scheduler::removeTask(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(scheduler_mutex_);

        // Find and remove the task
        auto it = std::find_if(all_tasks_.begin(), all_tasks_.end(),
                               [&](const TaskPtr &task)
                               { return task->getName() == name; });

        if (it != all_tasks_.end())
        {
            TaskPtr task = *it;
            task->terminate();

            // Remove from all_tasks_
            all_tasks_.erase(it);

            // Task symbols cleanup
            task_symbols_.erase(task);

            // Note: We don't remove from ready_queue_ here, but it will be filtered out
            // in selectNextTask() when its state is checked
        }
    }

    TaskPtr Scheduler::findTask(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(scheduler_mutex_);

        auto it = std::find_if(all_tasks_.begin(), all_tasks_.end(),
                               [&](const TaskPtr &task)
                               { return task->getName() == name; });

        if (it != all_tasks_.end())
        {
            return *it;
        }

        return nullptr;
    }

    void Scheduler::start()
    {
        if (!is_running_.exchange(true))
        {
            last_schedule_time_ = std::chrono::steady_clock::now();
            scheduler_thread_ = std::thread(&Scheduler::schedulerLoop, this);
            deadline_monitor_thread_ = std::thread(&Scheduler::deadlineMonitorLoop, this);
        }
    }

    void Scheduler::stop()
    {
        if (is_running_.exchange(false))
        {
            scheduler_cv_.notify_one();
            if (scheduler_thread_.joinable())
            {
                scheduler_thread_.join();
            }
            if (deadline_monitor_thread_.joinable())
            {
                deadline_monitor_thread_.join();
            }
        }
    }

    void Scheduler::yield()
    {
        // Cooperative yielding - a task voluntarily gives up the CPU
        force_reschedule_ = true;
        scheduler_cv_.notify_one();
    }

    void Scheduler::setPreemptionMode(PreemptionMode mode)
    {
        preemption_mode_ = mode;
    }

    void Scheduler::setTimeSlice(std::chrono::milliseconds time_slice)
    {
        time_slice_ = time_slice;
    }

    void Scheduler::adjustPriorities()
    {
        std::lock_guard<std::mutex> lock(scheduler_mutex_);

        for (auto &task : all_tasks_)
        {
            task->updatePriority();
        }

        // Rebuild ready queue with updated priorities
        std::priority_queue<TaskPtr, std::vector<TaskPtr>, TaskPriorityCompare> new_queue;

        while (!ready_queue_.empty())
        {
            auto task = ready_queue_.top();
            ready_queue_.pop();

            if (task->getState() == TaskState::READY)
            {
                new_queue.push(task);
            }
        }

        ready_queue_ = std::move(new_queue);
    }

    void Scheduler::updateCpuUtilization()
    {
        auto total_time = total_run_time_ + total_idle_time_;
        if (total_time.count() > 0)
        {
            cpu_utilization_ = static_cast<float>(total_run_time_.count()) / total_time.count() * 100.0f;
        }
        else
        {
            cpu_utilization_ = 0.0f;
        }
    }

    void Scheduler::schedulerLoop()
    {
        while (is_running_)
        {
            std::unique_lock<std::mutex> lock(scheduler_mutex_);

            // Check for deadline misses
            checkDeadlines();

            // Select the next task to execute
            auto previous_task = current_task_;
            current_task_ = selectNextTask();

            if (current_task_)
            {
                // Exit idle state if we were idle
                exitIdleState();

                // Execute the task
                auto start_time = std::chrono::steady_clock::now();

                // Unlock during task execution
                lock.unlock();

                // Set task to running state
                current_task_->setState(TaskState::RUNNING);

                // Execute the task
                current_task_->execute();

                lock.lock();

                auto end_time = std::chrono::steady_clock::now();
                auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
                    end_time - start_time);

                // Update task statistics
                current_task_->updateStatistics(execution_time);

                // Add to total run time
                total_run_time_ += execution_time;

                // Check if task failed and needs recovery
                if (current_task_->getState() == TaskState::TERMINATED &&
                    current_task_->isRecoverable())
                {
                    attemptTaskRecovery(current_task_);
                }

                // Update CPU utilization
                updateCpuUtilization();

                // Reset the reschedule flag
                force_reschedule_ = false;
            }
            else
            {
                // Enter idle state - no tasks to run
                enterIdleState();

                // Wait for a task to become ready or for a timeout
                scheduler_cv_.wait_for(lock, std::chrono::milliseconds(1));

                // Exit idle state and record idle time
                exitIdleState();
            }

            // Check if we should preempt the current task
            bool time_slice_expired = false;
            auto now = std::chrono::steady_clock::now();

            // Check if the time slice has expired for preemptive tasks
            if (current_task_ && current_task_->getPolicy() == SchedulePolicy::PREEMPTIVE &&
                (preemption_mode_ == PreemptionMode::TIME_SLICE || preemption_mode_ == PreemptionMode::HYBRID))
            {
                time_slice_expired = (now - last_schedule_time_ >= time_slice_);
            }

            // Reschedule if time slice expired or force_reschedule flag is set
            if (time_slice_expired || force_reschedule_)
            {
                last_schedule_time_ = now;
                force_reschedule_ = false;

                // If we have a current task, put it back in the ready queue
                if (current_task_ && current_task_->getState() == TaskState::READY)
                {
                    ready_queue_.push(current_task_);
                }
                current_task_ = nullptr;
            }

            // Periodically adjust priorities
            static auto last_priority_adjustment = std::chrono::steady_clock::now();
            if (now - last_priority_adjustment > std::chrono::seconds(1))
            {
                adjustPriorities();
                last_priority_adjustment = now;
            }
        }
    }

    void Scheduler::deadlineMonitorLoop()
    {
        while (is_running_)
        {
            // Sleep for a short time to avoid high CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // Get elapsed time since last check
            static auto last_check_time = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check_time);
            last_check_time = now;

            // Update deadline counters for all tasks
            {
                std::lock_guard<std::mutex> lock(scheduler_mutex_);

                for (auto &task : all_tasks_)
                {
                    // Only update deadline counters for tasks that aren't currently running
                    if (task != current_task_ || task->getState() != TaskState::RUNNING)
                    {
                        task->updateDeadlineCounter(elapsed);
                    }

                    // If a high priority task's deadline is approaching, we may need to preempt
                    if (task->isDeadlineApproaching() &&
                        task->getState() == TaskState::READY &&
                        current_task_ &&
                        task->getDynamicPriority() > current_task_->getDynamicPriority() &&
                        (preemption_mode_ == PreemptionMode::PRIORITY || preemption_mode_ == PreemptionMode::HYBRID))
                    {
                        // Signal that we should reschedule
                        force_reschedule_ = true;
                        scheduler_cv_.notify_one();
                    }
                }
            }
        }
    }

    TaskPtr Scheduler::selectNextTask()
    {
        // If ready queue is empty, rebuild it from all_tasks_
        if (ready_queue_.empty())
        {
            for (auto &task : all_tasks_)
            {
                if (task->getState() == TaskState::READY)
                {
                    ready_queue_.push(task);
                }
            }
        }

        // If still empty, no tasks are ready
        if (ready_queue_.empty())
        {
            return nullptr;
        }

        // Get the highest priority task
        TaskPtr next_task = ready_queue_.top();
        ready_queue_.pop();

        // Double-check task state
        if (next_task->getState() != TaskState::READY)
        {
            return selectNextTask(); // Recursively find the next ready task
        }

        return next_task;
    }

    void Scheduler::checkDeadlines()
    {
        auto now = std::chrono::steady_clock::now();

        for (auto &task : all_tasks_)
        {
            if (task->getDeadline().count() > 0 && task->getPeriod().count() > 0)
            {
                auto last_exec = task->getStatistics().last_execution;
                if (last_exec.time_since_epoch().count() > 0)
                { // If task has executed at least once
                    auto time_since_last_exec = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - last_exec);

                    if (time_since_last_exec > task->getPeriod() + task->getDeadline())
                    {
                        task->recordDeadlineMiss();
                    }
                }
            }
        }
    }

    bool Scheduler::shouldPreempt(TaskPtr new_task) const
    {
        // If there's no current task, no need to preempt
        if (!current_task_)
            return false;

        // If the current task is cooperative, don't preempt
        if (current_task_->getPolicy() == SchedulePolicy::COOPERATIVE)
            return false;

        // Check based on preemption mode
        switch (preemption_mode_)
        {
        case PreemptionMode::NONE:
            return false;

        case PreemptionMode::PRIORITY:
            // Preempt if new task has higher priority
            return new_task->getDynamicPriority() > current_task_->getDynamicPriority();

        case PreemptionMode::TIME_SLICE:
            // Time slice preemption is handled in the scheduler loop
            return false;

        case PreemptionMode::HYBRID:
            // Preempt if new task has higher priority
            return new_task->getDynamicPriority() > current_task_->getDynamicPriority();
        }

        return false;
    }

    bool Scheduler::attemptTaskRecovery(TaskPtr task)
    {
        if (!task->isRecoverable())
        {
            return false;
        }

        if (recovery_attempts_ >= MAX_RECOVERY_ATTEMPTS)
        {
            std::cerr << "Max recovery attempts reached for task: " << task->getName() << std::endl;
            return false;
        }

        std::cout << "Attempting recovery for task: " << task->getName() << std::endl;
        recovery_attempts_++;

        // Set task back to READY state
        task->setState(TaskState::READY);
        ready_queue_.push(task);

        return true;
    }

    char Scheduler::getSymbolForTaskState(TaskState state)
    {
        switch (state)
        {
        case TaskState::READY:
            return '.';
        case TaskState::RUNNING:
            return 'R';
        case TaskState::BLOCKED:
            return 'B';
        case TaskState::SUSPENDED:
            return 'S';
        case TaskState::TERMINATED:
            return 'T';
        default:
            return '?';
        }
    }

    void Scheduler::printTaskStates()
    {
        std::cout << getTaskStateVisualization() << std::endl;
    }

    std::string Scheduler::getTaskStateVisualization()
    {
        std::lock_guard<std::mutex> lock(scheduler_mutex_);
        std::stringstream ss;

        // Ensure we have tasks to visualize
        if (all_tasks_.empty())
        {
            return "No tasks registered in the scheduler.";
        }

        // Create a map for task symbols if it doesn't exist
        if (task_symbols_.empty())
        {
            char symbol = 'A';
            for (const auto &task : all_tasks_)
            {
                task_symbols_[task] = symbol++;
            }
        }

        // Header row with task symbols
        ss << "Time | ";
        for (const auto &task : all_tasks_)
        {
            auto it = task_symbols_.find(task);
            char symbol = (it != task_symbols_.end()) ? it->second : '?';
            ss << symbol << " ";
        }
        ss << "| Tasks\n";

        // Separator
        ss << "-----|-";
        for (size_t i = 0; i < all_tasks_.size(); i++)
        {
            ss << "--";
        }
        ss << "|---------\n";

        // Current state
        ss << "now  | ";
        for (const auto &task : all_tasks_)
        {
            ss << getSymbolForTaskState(task->getState()) << " ";
        }
        ss << "| ";

        // Print task names and priorities
        bool first = true;
        for (const auto &task : all_tasks_)
        {
            if (!first)
                ss << ", ";
            first = false;

            auto it = task_symbols_.find(task);
            char symbol = (it != task_symbols_.end()) ? it->second : '?';

            ss << symbol << ":" << task->getName()
               << "(" << static_cast<int>(task->getDynamicPriority()) << ")";
        }

        return ss.str();
    }

    void Scheduler::enterIdleState()
    {
        idle_start_time_ = std::chrono::steady_clock::now();
    }

    void Scheduler::exitIdleState()
    {
        auto now = std::chrono::steady_clock::now();
        auto idle_time = std::chrono::duration_cast<std::chrono::microseconds>(now - idle_start_time_);
        total_idle_time_ += idle_time;
    }

} // namespace edurtos
