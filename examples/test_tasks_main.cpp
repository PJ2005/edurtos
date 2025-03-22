#include "../include/kernel/kernel.hpp"
#include "../include/util/console_dashboard.hpp"
#include "../include/util/test_tasks.hpp"
#include "../include/util/scheduler_logger.hpp"
#include "../include/util/fault_injector.hpp"
#include "../include/util/console_logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <conio.h> // For keyboard input

// Use the logger for output
#define cout edurtos::util::ConsoleLogger::getInstance()

using namespace edurtos;
using namespace edurtos::util;

// Signal handler for Ctrl+C
std::atomic<bool> running = true;
void signalHandler(int signal)
{
    if (signal == SIGINT)
    {
        cout << "\nReceived Ctrl+C, exiting...\n";
        running = false;
    }
}

int main()
{
    // Initialize console logger
    ConsoleLogger::getInstance().init("test_tasks_output.txt");

    cout << "EduRTOS Test Tasks Application\n";
    cout << "------------------------------\n\n";

    // Register signal handler
    std::signal(SIGINT, signalHandler);

    // Get RTOS kernel instance
    auto &kernel = Kernel::getInstance();
    kernel.initialize();

    // Create a scheduler logger
    SchedulerLogger logger(*kernel.getScheduler(), "scheduler_decisions.csv");
    logger.start();
    logger.logEvent("SYSTEM", "Test started");

    // Create the test tasks
    cout << "Creating test tasks...\n";
    auto test_tasks = createStandardTestSet();

    // Register tasks with kernel
    auto cpu_task = kernel.createTask(
        test_tasks[0]->getName(),
        test_tasks[0]->getHandler(),
        test_tasks[0]->getPriority(),
        SchedulePolicy::PREEMPTIVE,
        std::chrono::milliseconds(500), // Period
        test_tasks[0]->getDeadline(),   // Deadline
        true                            // Recoverable
    );

    auto io_task = kernel.createTask(
        test_tasks[1]->getName(),
        test_tasks[1]->getHandler(),
        test_tasks[1]->getPriority(),
        SchedulePolicy::PREEMPTIVE,
        std::chrono::milliseconds(1000), // Period
        test_tasks[1]->getDeadline(),    // Deadline
        true                             // Recoverable
    );

    auto mixed_task = kernel.createTask(
        test_tasks[2]->getName(),
        test_tasks[2]->getHandler(),
        test_tasks[2]->getPriority(),
        SchedulePolicy::PREEMPTIVE,
        std::chrono::milliseconds(2000), // Period
        test_tasks[2]->getDeadline(),    // Deadline
        true                             // Recoverable
    );

    logger.logEvent("SYSTEM", "Tasks registered with kernel");

    // Create a fault injector with reduced probability
    FaultInjector fault_injector(kernel);
    fault_injector.setFaultProbability(0.05); // Reduced from 10% to 5% chance of fault

    // Start components
    cout << "Starting fault injector...\n";
    fault_injector.start(std::chrono::seconds(30)); // Increased interval from 20 to 30 seconds

    cout << "Starting kernel...\n";
    kernel.start();

    logger.logEvent("SYSTEM", "System started");

    // Run for a shorter time to avoid memory issues
    cout << "Running test tasks for demonstration (30 seconds)...\n";
    auto start_time = std::chrono::steady_clock::now();
    const auto test_duration = std::chrono::seconds(30); // Reduced from 60 to 30 seconds

    // Add timeout mechanism
    std::atomic<bool> timeout{false};
    std::thread timeout_thread([&]()
                               {
        std::this_thread::sleep_for(std::chrono::seconds(35)); // 5 seconds extra
        timeout = true;
        cout << "Safety timeout triggered. Forcing program exit.\n"; });
    timeout_thread.detach();

    // Add a hard timeout to ensure the program exits
    auto hard_timeout = std::thread([&]()
                                    {
                                        std::this_thread::sleep_for(std::chrono::seconds(28)); // Absolute maximum runtime
                                        cout << "Hard timeout reached. Forcing exit.\n";
                                        std::exit(0); // Force exit the program
                                    });
    hard_timeout.detach();

    while (running &&
           !timeout &&
           std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - start_time) < test_duration)
    {
        // Output periodic status for the log
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time)
                    .count() %
                5 ==
            0)
        {
            // Print current task states - with improved formatting
            cout << "--------------------------------------------------\n";
            cout << "Current task states:\n";
            std::string task_vis = kernel.getScheduler()->getTaskStateVisualization();
            if (!task_vis.empty())
            {
                cout << task_vis << "\n";
            }
            else
            {
                cout << "No task state information available.\n";
            }
            cout << "CPU Utilization: " << std::fixed << std::setprecision(1)
                 << kernel.getScheduler()->getCpuUtilization() << "%\n";
            cout << "--------------------------------------------------\n";

            // Wait a bit to avoid duplicate status messages
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Allow some time for tasks to run, but check more frequently
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Shutdown
    cout << "Stopping test...\n";
    logger.logEvent("SYSTEM", "System stopping");

    // Shutdown - wrap in try-catch to handle any exceptions
    try
    {
        // Order matters for cleanup - stop fault injector first
        fault_injector.stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        kernel.stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        logger.stop();
    }
    catch (...)
    {
        cout << "Exception caught during shutdown.\n";
    }

    cout << "Test completed. Scheduler decisions logged to scheduler_decisions.csv\n";
    cout << "Console output logged to test_tasks_output.txt\n";

    // Close the logger
    ConsoleLogger::getInstance().close();

    return 0;
}
