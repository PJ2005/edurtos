#pragma once

#include "../kernel/kernel.hpp"
#include <atomic>
#include <random>
#include <thread>
#include <functional>
#include <map>
#include <mutex>
#include <csignal>
#include <unordered_map>
#include <condition_variable>

namespace edurtos
{
    namespace util
    {

        // Thread-local storage for tracking task execution context
        struct TaskExecutionContext
        {
            TaskPtr current_task;
            bool in_protected_region;
            void *stack_checkpoint;
            size_t stack_checkpoint_size;
        };

        // Class for injecting faults into the RTOS
        class FaultInjector
        {
        public:
            enum class FaultType
            {
                STACK_CORRUPTION,  // Corrupt task stack
                DEADLOCK,          // Simulate deadlock condition
                NULL_POINTER,      // Simulate null pointer dereference
                INFINITE_LOOP,     // Simulate infinite loop
                MEMORY_LEAK,       // Simulate memory leak
                SEGMENTATION_FAULT // Simulate segmentation fault
            };

            // Constructor takes a reference to the kernel
            FaultInjector(Kernel &kernel);
            ~FaultInjector();

            // Start/stop fault injection
            void start(std::chrono::seconds injection_interval = std::chrono::seconds(30));
            void stop();

            // Configure fault injection
            void setFaultProbability(double probability);
            void enableFaultType(FaultType type, bool enable);
            void setFaultTypeWeight(FaultType type, double weight);

            // Manually inject a specific fault
            bool injectFault(FaultType type, const std::string &target_task_name = "");

            // Signal handler registration
            static void setupSignalHandlers();

            // Thread checkpoint functions
            static void beginProtectedRegion(TaskPtr task);
            static void endProtectedRegion();
            static void createCheckpoint(TaskPtr task);
            static bool restoreFromCheckpoint();

        private:
            // Reference to kernel
            Kernel &kernel_;

            // Fault injection settings
            double fault_probability_{0.1}; // Default 10% chance of fault
            std::map<FaultType, bool> enabled_faults_;
            std::map<FaultType, double> fault_weights_;

            // Thread-local storage for task context
            static thread_local TaskExecutionContext thread_context_;

            // Thread management
            std::atomic<bool> is_running_{false};
            std::thread injection_thread_;
            std::chrono::seconds injection_interval_;

            // Thread checkpoint map
            std::unordered_map<std::string, std::pair<void *, size_t>> task_checkpoints_;
            std::mutex checkpoint_mutex_;

            // Fault injection methods
            void faultInjectionLoop();
            FaultType selectRandomFaultType();
            TaskPtr selectRandomTask();

            // Specific fault injection methods
            bool injectStackCorruption(TaskPtr task);
            bool injectDeadlock(TaskPtr task);
            bool injectNullPointer(TaskPtr task);
            bool injectInfiniteLoop(TaskPtr task);
            bool injectMemoryLeak(TaskPtr task);
            bool injectSegmentationFault(TaskPtr task);

            // Signal handling
            static void handleSegmentationFault(int signal);
            static void restartTask(TaskPtr task);

            // Random number generation
            std::mt19937 rng_{std::random_device{}()};
        };

    } // namespace util
} // namespace edurtos
