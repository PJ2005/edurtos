#include "../../include/util/fault_injector.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

namespace edurtos
{
    namespace util
    {

        // Initialize thread-local storage
        thread_local TaskExecutionContext FaultInjector::thread_context_ = {
            nullptr, // current_task
            false,   // in_protected_region
            nullptr, // stack_checkpoint
            0        // stack_checkpoint_size
        };

        // Signal handler function pointer
        static void (*original_segv_handler)(int) = nullptr;

        FaultInjector::FaultInjector(Kernel &kernel)
            : kernel_(kernel)
        {

            // Initialize fault types with default settings
            enabled_faults_[FaultType::STACK_CORRUPTION] = true;
            enabled_faults_[FaultType::DEADLOCK] = true;
            enabled_faults_[FaultType::NULL_POINTER] = true;
            enabled_faults_[FaultType::INFINITE_LOOP] = true;
            enabled_faults_[FaultType::MEMORY_LEAK] = true;
            enabled_faults_[FaultType::SEGMENTATION_FAULT] = true;

            // Set default weights
            fault_weights_[FaultType::STACK_CORRUPTION] = 2.0;
            fault_weights_[FaultType::DEADLOCK] = 1.0;
            fault_weights_[FaultType::NULL_POINTER] = 1.0;
            fault_weights_[FaultType::INFINITE_LOOP] = 0.5;
            fault_weights_[FaultType::MEMORY_LEAK] = 0.5;
            fault_weights_[FaultType::SEGMENTATION_FAULT] = 1.0;

            // Register signal handlers
            setupSignalHandlers();
        }

        FaultInjector::~FaultInjector()
        {
            stop();

            // Clean up any allocated checkpoints
            std::lock_guard<std::mutex> lock(checkpoint_mutex_);
            for (auto &[task_name, checkpoint] : task_checkpoints_)
            {
                if (checkpoint.first)
                {
                    free(checkpoint.first);
                }
            }
            task_checkpoints_.clear();

            // Restore original signal handlers
#ifndef _WIN32
            signal(SIGSEGV, original_segv_handler);
#endif
        }

        void FaultInjector::setupSignalHandlers()
        {
            // Save original handlers and set new ones
#ifndef _WIN32
            original_segv_handler = signal(SIGSEGV, handleSegmentationFault);
#endif
        }

        void FaultInjector::start(std::chrono::seconds injection_interval)
        {
            if (!is_running_.exchange(true))
            {
                injection_interval_ = injection_interval;
                injection_thread_ = std::thread(&FaultInjector::faultInjectionLoop, this);
            }
        }

        void FaultInjector::stop()
        {
            if (is_running_.exchange(false))
            {
                if (injection_thread_.joinable())
                {
                    injection_thread_.join();
                }
            }
        }

        void FaultInjector::setFaultProbability(double probability)
        {
            fault_probability_ = std::max(0.0, std::min(1.0, probability));
        }

        void FaultInjector::enableFaultType(FaultType type, bool enable)
        {
            enabled_faults_[type] = enable;
        }

        void FaultInjector::setFaultTypeWeight(FaultType type, double weight)
        {
            fault_weights_[type] = std::max(0.0, weight);
        }

        FaultInjector::FaultType FaultInjector::selectRandomFaultType()
        {
            std::vector<FaultType> enabled_types;
            std::vector<double> weights;

            for (const auto &[type, enabled] : enabled_faults_)
            {
                if (enabled)
                {
                    enabled_types.push_back(type);
                    weights.push_back(fault_weights_[type]);
                }
            }

            if (enabled_types.empty())
            {
                return FaultType::STACK_CORRUPTION; // Default
            }

            std::discrete_distribution<size_t> dist(weights.begin(), weights.end());
            size_t index = dist(rng_);

            return enabled_types[index];
        }

        TaskPtr FaultInjector::selectRandomTask()
        {
            auto *scheduler = kernel_.getScheduler();
            if (!scheduler)
                return nullptr;

            const auto &tasks = scheduler->getAllTasks();
            if (tasks.empty())
                return nullptr;

            std::uniform_int_distribution<size_t> dist(0, tasks.size() - 1);
            return tasks[dist(rng_)];
        }

        void FaultInjector::faultInjectionLoop()
        {
            while (is_running_)
            {
                std::this_thread::sleep_for(injection_interval_);

                // Decide whether to inject a fault based on probability
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                if (dist(rng_) < fault_probability_)
                {
                    FaultType fault_type = selectRandomFaultType();
                    TaskPtr target_task = selectRandomTask();

                    if (target_task)
                    {
                        injectFault(fault_type, target_task->getName());
                    }
                }
            }
        }

        bool FaultInjector::injectFault(FaultType type, const std::string &target_task_name)
        {
            TaskPtr target_task;

            if (!target_task_name.empty())
            {
                target_task = kernel_.getTask(target_task_name);
                if (!target_task)
                {
                    std::cerr << "Error: Unable to find task '" << target_task_name
                              << "' for fault injection." << std::endl;
                    return false;
                }
            }
            else
            {
                target_task = selectRandomTask();
                if (!target_task)
                {
                    std::cerr << "Error: No tasks available for fault injection." << std::endl;
                    return false;
                }
            }

            std::cout << "Injecting fault: ";
            switch (type)
            {
            case FaultType::STACK_CORRUPTION:
                std::cout << "STACK_CORRUPTION";
                return injectStackCorruption(target_task);
            case FaultType::DEADLOCK:
                std::cout << "DEADLOCK";
                return injectDeadlock(target_task);
            case FaultType::NULL_POINTER:
                std::cout << "NULL_POINTER";
                return injectNullPointer(target_task);
            case FaultType::INFINITE_LOOP:
                std::cout << "INFINITE_LOOP";
                return injectInfiniteLoop(target_task);
            case FaultType::MEMORY_LEAK:
                std::cout << "MEMORY_LEAK";
                return injectMemoryLeak(target_task);
            case FaultType::SEGMENTATION_FAULT:
                std::cout << "SEGMENTATION_FAULT";
                return injectSegmentationFault(target_task);
            default:
                std::cout << "UNKNOWN_FAULT";
                return false;
            }
        }

        bool FaultInjector::injectStackCorruption(TaskPtr task)
        {
            std::cout << " into task: " << task->getName() << std::endl;

            // Note: Real stack corruption would require access to the actual task stack,
            // which isn't directly accessible in this simulator. Instead, we'll
            // simulate the effect by modifying the checkpoint if one exists.

            std::lock_guard<std::mutex> lock(checkpoint_mutex_);
            auto it = task_checkpoints_.find(task->getName());
            if (it != task_checkpoints_.end())
            {
                if (it->second.first && it->second.second > 0)
                {
                    // Corrupt a random part of the checkpoint data
                    uint8_t *data = static_cast<uint8_t *>(it->second.first);
                    size_t offset = std::uniform_int_distribution<size_t>(0, it->second.second - 1)(rng_);
                    data[offset] = std::uniform_int_distribution<uint8_t>(0, 255)(rng_);

                    std::cout << "Corrupted checkpoint for task: " << task->getName()
                              << " at offset: " << offset << std::endl;
                    return true;
                }
            }

            std::cerr << "No checkpoint available for task: " << task->getName() << std::endl;
            return false;
        }

        bool FaultInjector::injectDeadlock(TaskPtr task)
        {
            std::cout << " into task: " << task->getName() << std::endl;

            // Simulate a deadlock by suspending the task
            // In a real system, this would involve creating an actual deadlock condition
            task->suspend();

            std::cout << "Task " << task->getName() << " suspended to simulate deadlock" << std::endl;
            return true;
        }

        bool FaultInjector::injectNullPointer(TaskPtr task)
        {
            std::cout << " into task: " << task->getName() << std::endl;

            // Can't actually dereference a null pointer directly, so we'll simulate it
            // by manually triggering a segmentation fault

#ifdef _WIN32
            std::cerr << "NULL_POINTER fault simulation not supported on Windows" << std::endl;
            return false;
#else
            // Create a function that will be executed next time the task runs
            auto original_handler = task->getHandler();
            auto null_pointer_handler = [original_handler]()
            {
                // First set the context so we know which task is running
                TaskExecutionContext &ctx = FaultInjector::thread_context_;
                ctx.in_protected_region = true;

                // Trigger a segmentation fault
                int *null_ptr = nullptr;
                *null_ptr = 42; // This will cause SIGSEGV

                // This won't be reached, but including for completeness
                original_handler();
            };

            // Register our null pointer handler
            // Note: In a real implementation, you'd need a way to modify the task's handler
            // which isn't directly accessible in this simulator
            std::cerr << "NULL_POINTER fault simulation requires direct handler modification" << std::endl;
            return false;
#endif
        }

        bool FaultInjector::injectInfiniteLoop(TaskPtr task)
        {
            std::cout << " into task: " << task->getName() << std::endl;

            // Simulate an infinite loop by replacing the task's handler
            // In a real system, this would involve modifying the actual task code

            // Not directly implementable in this simulator without handler modification
            std::cerr << "INFINITE_LOOP fault simulation requires handler modification" << std::endl;
            return false;
        }

        bool FaultInjector::injectMemoryLeak(TaskPtr task)
        {
            std::cout << " into task: " << task->getName() << std::endl;

            // Simulate a memory leak by allocating memory that won't be freed
            // This is not immediately destructive, but will eventually cause problems
            try
            {
                // Allocate smaller memory to avoid causing actual problems
                // Reduced from 1024-4096 to 256-512 bytes
                size_t leak_size = std::uniform_int_distribution<size_t>(256, 512)(rng_);
                void *leak = malloc(leak_size);
                if (!leak)
                {
                    std::cerr << "Failed to allocate memory for leak simulation" << std::endl;
                    return false;
                }

                // Fill with recognizable pattern
                std::memset(leak, 0xDE, leak_size);

                std::cout << "Leaked " << leak_size << " bytes in task: "
                          << task->getName() << std::endl;

                // Add a delay to ensure console output is flushed
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error injecting memory leak: " << e.what() << std::endl;
                return false;
            }
        }

        bool FaultInjector::injectSegmentationFault(TaskPtr task)
        {
            std::cout << " into task: " << task->getName() << std::endl;

            // Similar to null pointer, this would need to modify the task's execution
            // In this simulator, we'll just print a message
            std::cerr << "SEGMENTATION_FAULT simulation requires handler modification" << std::endl;
            return false;
        }

        void FaultInjector::beginProtectedRegion(TaskPtr task)
        {
            thread_context_.current_task = task;
            thread_context_.in_protected_region = true;
        }

        void FaultInjector::endProtectedRegion()
        {
            thread_context_.in_protected_region = false;
        }

        void FaultInjector::createCheckpoint(TaskPtr task)
        {
            if (!task)
                return;

            // In a real system, this would capture the task's stack and registers
            // For simulation, we'll just allocate a dummy checkpoint

            static FaultInjector *instance = nullptr;
            if (!instance)
                return;

            std::lock_guard<std::mutex> lock(instance->checkpoint_mutex_);

            // Clean up previous checkpoint if it exists
            auto it = instance->task_checkpoints_.find(task->getName());
            if (it != instance->task_checkpoints_.end() && it->second.first)
            {
                free(it->second.first);
            }

            // Create a dummy checkpoint (in a real system this would be the actual stack)
            size_t checkpoint_size = 4096; // Arbitrary size
            void *checkpoint_data = malloc(checkpoint_size);

            if (!checkpoint_data)
            {
                std::cerr << "Failed to allocate memory for checkpoint" << std::endl;
                return;
            }

            // Store in checkpoint map
            instance->task_checkpoints_[task->getName()] = {checkpoint_data, checkpoint_size};

            std::cout << "Created checkpoint for task: " << task->getName() << std::endl;
        }

        bool FaultInjector::restoreFromCheckpoint()
        {
            TaskPtr task = thread_context_.current_task;
            if (!task)
                return false;

            static FaultInjector *instance = nullptr;
            if (!instance)
                return false;

            std::lock_guard<std::mutex> lock(instance->checkpoint_mutex_);

            auto it = instance->task_checkpoints_.find(task->getName());
            if (it == instance->task_checkpoints_.end() || !it->second.first)
            {
                std::cerr << "No checkpoint available for task: "
                          << (task ? task->getName() : "unknown") << std::endl;
                return false;
            }

            // In a real system, this would restore the task's stack and registers
            std::cout << "Restored checkpoint for task: " << task->getName() << std::endl;

            return true;
        }

        void FaultInjector::handleSegmentationFault(int signal)
        {
            std::cerr << "Caught segmentation fault (SIGSEGV)" << std::endl;

            // Check if we're in a protected region and have a task
            if (thread_context_.in_protected_region && thread_context_.current_task)
            {
                std::cerr << "Fault occurred in protected region for task: "
                          << thread_context_.current_task->getName() << std::endl;

                // Try to restore from checkpoint
                if (restoreFromCheckpoint())
                {
                    std::cerr << "Successfully restored from checkpoint" << std::endl;
                    return; // Continue execution
                }

                // If restore failed, try to restart the task
                std::cerr << "Checkpoint restore failed, trying to restart task" << std::endl;
                restartTask(thread_context_.current_task);
            }

            // If we're not in a protected region or couldn't handle it, call the original handler
            if (original_segv_handler)
            {
                original_segv_handler(signal);
            }
            else
            {
                // Default handler behavior
                std::cerr << "Unhandled segmentation fault, terminating" << std::endl;
                std::exit(EXIT_FAILURE);
            }
        }

        void FaultInjector::restartTask(TaskPtr task)
        {
            if (!task)
                return;

            // In a real system, this would involve reinitializing the task
            // For this simulator, we'll just reset its state
            if (task->getState() == TaskState::TERMINATED ||
                task->getState() == TaskState::SUSPENDED)
            {
                task->resume();
                std::cout << "Task " << task->getName() << " restarted after fault" << std::endl;
            }
        }

    } // namespace util
} // namespace edurtos
