#include "../include/kernel/kernel.hpp"
#include "../include/util/console_visualizer.hpp"
#include "../include/util/console_dashboard.hpp"
#include "../include/drivers/virtual_hardware.hpp"
#include "../include/util/scheduler_logger.hpp"
#include "../include/util/fault_injector.hpp"
#include "../include/util/console_logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <conio.h> // For keyboard input

// Use the logger for output
#define cout edurtos::util::ConsoleLogger::getInstance()

// Global flag to control test duration
std::atomic<bool> running = true;

// Example periodic task that increments a counter
void periodicTask()
{
    static int counter = 0;
    cout << "Periodic task executed. Counter: " << ++counter << std::endl;

    // Use virtual hardware
    auto &hal = edurtos::drivers::HAL::getInstance();
    hal.getUART().transmit("Periodic task tick: " + std::to_string(counter));

    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

// Example CPU-intensive task that occasionally misses deadlines
void cpuIntensiveTask()
{
    static int iterations = 0;
    cout << "CPU intensive task started. Iteration: " << ++iterations << std::endl;

    // Simulate varying workload
    int work = (iterations % 5 == 0) ? 150 : 30; // Every 5th iteration takes longer
    std::this_thread::sleep_for(std::chrono::milliseconds(work));

    // Blink a virtual LED
    auto &gpio = edurtos::drivers::HAL::getInstance().getGPIO();
    static bool led_state = false;
    gpio.writePin(5, led_state = !led_state);

    cout << "CPU intensive task completed." << std::endl;
}

// Example recoverable task that occasionally fails
void recoverableTask()
{
    static int runs = 0;
    runs++;

    cout << "Recoverable task running. Attempt: " << runs << std::endl;

    // Simulate occasional failure
    if (runs % 3 == 0)
    {
        cout << "Recoverable task throwing exception!" << std::endl;
        throw std::runtime_error("Simulated task failure");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    cout << "Recoverable task completed successfully." << std::endl;
}

// Example cooperative task that yields periodically
void cooperativeTask()
{
    cout << "Cooperative task starting..." << std::endl;

    // Do some work in chunks, yielding between chunks
    for (int i = 0; i < 5; i++)
    {
        cout << "Cooperative task - work chunk " << i + 1 << " of 5" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Manually yield to scheduler
        if (i < 4)
        {
            cout << "Cooperative task yielding..." << std::endl;
            edurtos::Kernel::getInstance().getScheduler()->yield(); // Fixed: properly call yield
        }
    }

    cout << "Cooperative task completed." << std::endl;
}

int main()
{
    // Initialize console logger
    edurtos::util::ConsoleLogger::getInstance().init("edurtos_output.txt");

    cout << "EduRTOS Example Application with Fault Injection\n";
    cout << "----------------------------------------------\n";

    // Initialize HAL
    auto &hal = edurtos::drivers::HAL::getInstance();

    // Configure virtual hardware
    hal.getGPIO().setPinMode(5, edurtos::drivers::VirtualGPIO::PinMode::OUTPUT); // LED pin
    hal.getUART().configure(edurtos::drivers::VirtualUART::BaudRate::BAUD_115200);

    // Get RTOS kernel instance
    auto &kernel = edurtos::Kernel::getInstance();

    // Initialize kernel
    kernel.initialize();

    // Create scheduler logger
    edurtos::util::SchedulerLogger logger(*kernel.getScheduler());
    logger.start();
    logger.logEvent("SYSTEM", "Example application started");

    // Create tasks with different priorities (1-99 scale)
    auto periodic_task = kernel.createTask("Periodic", periodicTask, 50,
                                           edurtos::SchedulePolicy::PREEMPTIVE,
                                           std::chrono::milliseconds(100), // Run every 100ms
                                           std::chrono::milliseconds(90)); // Deadline of 90ms

    auto cpu_task = kernel.createTask("CPUIntensive", cpuIntensiveTask, 30,
                                      edurtos::SchedulePolicy::PREEMPTIVE,
                                      std::chrono::milliseconds(200),  // Run every 200ms
                                      std::chrono::milliseconds(100)); // Deadline of 100ms

    auto recover_task = kernel.createTask("Recoverable", recoverableTask, 70,
                                          edurtos::SchedulePolicy::PREEMPTIVE,
                                          std::chrono::milliseconds(300), // Run every 300ms
                                          std::chrono::milliseconds(50),  // Deadline of 50ms
                                          true);                          // Recoverable task

    auto coop_task = kernel.createTask("Cooperative", cooperativeTask, 40,
                                       edurtos::SchedulePolicy::COOPERATIVE,
                                       std::chrono::milliseconds(500)); // Run every 500ms

    logger.logEvent("SYSTEM", "Tasks created");

    // Skip dashboard for console output logging
    cout << "Starting kernel and tasks...\n";

    // Create fault injector with reduced probability
    edurtos::util::FaultInjector fault_injector(kernel);
    fault_injector.setFaultProbability(0.02); // Lower probability - only 2% chance

    // Disable memory leak faults to prevent system issues
    fault_injector.enableFaultType(edurtos::util::FaultInjector::FaultType::MEMORY_LEAK, false);
    fault_injector.start(std::chrono::seconds(45)); // Inject faults less frequently

    // Start the kernel
    kernel.start();
    logger.logEvent("SYSTEM", "Kernel started");

    // Add timeout mechanism
    std::atomic<bool> timeout{false};
    std::thread timeout_thread([&]()
                               {
        std::this_thread::sleep_for(std::chrono::seconds(35));
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

    // Run for a fixed time to capture output
    cout << "Running RTOS for demonstration (30 seconds)...\n";
    auto start_time = std::chrono::steady_clock::now();

    while (!timeout && std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::steady_clock::now() - start_time)
                               .count() < 30)
    {
        // Update the timer
        hal.getTimer().update();

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

        // Check for keypress (non-blocking) to allow early termination
        if (_kbhit())
        {
            char c = _getch();
            cout << "Key pressed. Early termination.\n";
            break;
        }

        // Small delay between updates, but check more frequently
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Stop everything - order of shutdown matters
    cout << "Test run completed. Stopping system...\n";
    logger.logEvent("SYSTEM", "System stopping");

    try
    {
        // Stop components in proper order with delays
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

    cout << "RTOS test completed.\n";
    cout << "Scheduler decisions logged to scheduler_log.csv\n";
    cout << "Console output logged to edurtos_output.txt\n";

    // Close the logger
    edurtos::util::ConsoleLogger::getInstance().close();

    return 0;
}
