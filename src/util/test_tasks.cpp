#include "../../include/util/test_tasks.hpp"
#include <algorithm>
#include <iostream>
#include <thread>
#include <cmath>

namespace edurtos
{
    namespace util
    {

        TestTask::TestTask(const std::string &name,
                           ExecutionPattern pattern,
                           uint8_t priority,
                           std::chrono::milliseconds deadline)
            : name_(name),
              pattern_(pattern),
              priority_(priority),
              deadline_(deadline)
        {
            // Initialize work buffer for CPU-bound tasks
            work_buffer_.resize(1000, 0);
            std::iota(work_buffer_.begin(), work_buffer_.end(), 0);
        }

        TestTask::~TestTask() = default;

        std::function<void()> TestTask::getHandler()
        {
            return [this]()
            {
                incrementExecutionCount();

                switch (pattern_)
                {
                case ExecutionPattern::CPU_BOUND:
                    executeCpuBound();
                    break;
                case ExecutionPattern::IO_BOUND:
                    executeIoBound();
                    break;
                case ExecutionPattern::MIXED:
                    executeMixed();
                    break;
                case ExecutionPattern::BURSTY:
                    executeBursty();
                    break;
                }
            };
        }

        void TestTask::executeCpuBound()
        {
            // CPU-bound task: performs heavy computation
            std::cout << name_ << ": Performing CPU-bound work..." << std::endl;

            // Shuffle the buffer to ensure the work isn't optimized away
            std::shuffle(work_buffer_.begin(), work_buffer_.end(), rng_);

            // Perform computationally intensive work
            simulateCpuWork(100000);

            std::cout << name_ << ": CPU-bound work completed." << std::endl;
        }

        void TestTask::executeIoBound()
        {
            // IO-bound task: performs operations with waiting time
            std::cout << name_ << ": Performing IO-bound work..." << std::endl;

            // Simulate multiple I/O operations
            for (int i = 0; i < 5; i++)
            {
                std::cout << name_ << ": IO operation " << i + 1 << " of 5" << std::endl;
                simulateIoWork(std::chrono::milliseconds(10));
            }

            std::cout << name_ << ": IO-bound work completed." << std::endl;
        }

        void TestTask::executeMixed()
        {
            // Mixed task: alternates between CPU and IO work
            std::cout << name_ << ": Performing mixed workload..." << std::endl;

            for (int i = 0; i < 3; i++)
            {
                // Some CPU-bound work
                std::cout << name_ << ": Computation phase " << i + 1 << std::endl;
                simulateCpuWork(30000);

                // Some IO-bound work
                std::cout << name_ << ": IO phase " << i + 1 << std::endl;
                simulateIoWork(std::chrono::milliseconds(5));
            }

            std::cout << name_ << ": Mixed work completed." << std::endl;
        }

        void TestTask::executeBursty()
        {
            // Bursty task: alternates between high and low activity
            std::cout << name_ << ": Performing bursty workload..." << std::endl;

            // Random decision whether this execution will be high or low intensity
            std::uniform_int_distribution<> dist(0, 100);
            int intensity = dist(rng_);

            if (intensity < 30)
            { // 30% chance of high intensity
                std::cout << name_ << ": High intensity burst" << std::endl;
                simulateCpuWork(150000);
            }
            else
            {
                std::cout << name_ << ": Low intensity work" << std::endl;
                simulateCpuWork(10000);
            }

            std::cout << name_ << ": Bursty work completed." << std::endl;
        }

        void TestTask::simulateCpuWork(size_t iterations)
        {
            // Perform computation that cannot be easily optimized away
            volatile double result = 0.0;

            for (size_t i = 0; i < iterations; i++)
            {
                result += std::sin(i * 0.01) * std::cos(i * 0.01);
            }
        }

        void TestTask::simulateIoWork(std::chrono::milliseconds duration)
        {
            // Simulate I/O work with a sleep
            std::this_thread::sleep_for(duration);
        }

        TestTaskPtr TestTask::createCpuBoundTask(const std::string &name,
                                                 uint8_t priority,
                                                 std::chrono::milliseconds deadline)
        {
            return std::make_shared<TestTask>(name, ExecutionPattern::CPU_BOUND, priority, deadline);
        }

        TestTaskPtr TestTask::createIoBoundTask(const std::string &name,
                                                uint8_t priority,
                                                std::chrono::milliseconds deadline)
        {
            return std::make_shared<TestTask>(name, ExecutionPattern::IO_BOUND, priority, deadline);
        }

        TestTaskPtr TestTask::createMixedTask(const std::string &name,
                                              uint8_t priority,
                                              std::chrono::milliseconds deadline)
        {
            return std::make_shared<TestTask>(name, ExecutionPattern::MIXED, priority, deadline);
        }

        TestTaskPtr TestTask::createBurstyTask(const std::string &name,
                                               uint8_t priority,
                                               std::chrono::milliseconds deadline)
        {
            return std::make_shared<TestTask>(name, ExecutionPattern::BURSTY, priority, deadline);
        }

        std::vector<TestTaskPtr> createStandardTestSet()
        {
            std::vector<TestTaskPtr> tasks;

            // Create the three requested test tasks with different characteristics
            tasks.push_back(TestTask::createCpuBoundTask("CPUBoundTask", 70, std::chrono::milliseconds(100)));
            tasks.push_back(TestTask::createIoBoundTask("IOBoundTask", 50, std::chrono::milliseconds(200)));
            tasks.push_back(TestTask::createMixedTask("MixedTask", 30, std::chrono::milliseconds(500)));

            return tasks;
        }

    } // namespace util
} // namespace edurtos
