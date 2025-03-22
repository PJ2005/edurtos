#pragma once

#include <functional>
#include <chrono>
#include <string>
#include <vector>
#include <random>
#include <memory>
#include <atomic>
#include <fstream>

namespace edurtos
{
    namespace util
    {

        // Execution pattern types
        enum class ExecutionPattern
        {
            CPU_BOUND, // Heavy computational workload
            IO_BOUND,  // I/O operations with waiting
            MIXED,     // Combination of CPU and I/O
            BURSTY     // Alternates between high and low activity
        };

        // Forward declaration
        class TestTask;
        using TestTaskPtr = std::shared_ptr<TestTask>;

        // Class that generates different execution patterns
        class TestTask
        {
        public:
            TestTask(const std::string &name,
                     ExecutionPattern pattern,
                     uint8_t priority,
                     std::chrono::milliseconds deadline);
            ~TestTask();

            // Task execution function
            std::function<void()> getHandler();

            // Task properties
            const std::string &getName() const { return name_; }
            ExecutionPattern getPattern() const { return pattern_; }
            uint8_t getPriority() const { return priority_; }
            std::chrono::milliseconds getDeadline() const { return deadline_; }

            // Monitoring
            void incrementExecutionCount() { execution_count_++; }
            size_t getExecutionCount() const { return execution_count_; }

            // Static factory methods for different patterns
            static TestTaskPtr createCpuBoundTask(const std::string &name,
                                                  uint8_t priority,
                                                  std::chrono::milliseconds deadline);

            static TestTaskPtr createIoBoundTask(const std::string &name,
                                                 uint8_t priority,
                                                 std::chrono::milliseconds deadline);

            static TestTaskPtr createMixedTask(const std::string &name,
                                               uint8_t priority,
                                               std::chrono::milliseconds deadline);

            static TestTaskPtr createBurstyTask(const std::string &name,
                                                uint8_t priority,
                                                std::chrono::milliseconds deadline);

        private:
            // Task properties
            std::string name_;
            ExecutionPattern pattern_;
            uint8_t priority_;
            std::chrono::milliseconds deadline_;

            // Tracking metrics
            std::atomic<size_t> execution_count_{0};

            // State for different execution patterns
            std::mt19937 rng_{std::random_device{}()};
            std::vector<int> work_buffer_;

            // Task implementation methods
            void executeCpuBound();
            void executeIoBound();
            void executeMixed();
            void executeBursty();

            // Utility methods
            void simulateCpuWork(size_t iterations);
            void simulateIoWork(std::chrono::milliseconds duration);
        };

        // Create a set of test tasks with different characteristics
        std::vector<TestTaskPtr> createStandardTestSet();

    } // namespace util
} // namespace edurtos
