#include "../../include/kernel/kernel.hpp"
#include <iostream>
#include <thread>

namespace edurtos
{

    Kernel *Kernel::instance_ = nullptr;

    Kernel::Kernel()
    {
        scheduler_ = std::make_unique<Scheduler>();
    }

    Kernel::~Kernel()
    {
        stop();
    }

    Kernel &Kernel::getInstance()
    {
        if (!instance_)
        {
            instance_ = new Kernel();
        }
        return *instance_;
    }

    void Kernel::initialize()
    {
        std::lock_guard<std::mutex> lock(kernel_mutex_);
        std::cout << "EduRTOS Initializing...\n";
    }

    void Kernel::start()
    {
        std::lock_guard<std::mutex> lock(kernel_mutex_);
        std::cout << "EduRTOS Starting...\n";
        scheduler_->start();

        if (auto_visualization_)
        {
            std::thread visualization_thread(&Kernel::visualizationLoop, this);
            visualization_thread.detach();
        }
    }

    void Kernel::stop()
    {
        std::lock_guard<std::mutex> lock(kernel_mutex_);
        std::cout << "EduRTOS Stopping...\n";
        scheduler_->stop();
        auto_visualization_ = false;
    }

    TaskPtr Kernel::createTask(const std::string &name,
                               std::function<void()> handler,
                               std::uint8_t priority,
                               SchedulePolicy policy,
                               std::chrono::milliseconds period,
                               std::chrono::milliseconds deadline,
                               bool recoverable)
    {
        std::lock_guard<std::mutex> lock(kernel_mutex_);

        if (tasks_.find(name) != tasks_.end())
        {
            std::cerr << "Task '" << name << "' already exists\n";
            return nullptr;
        }

        auto task = std::make_shared<Task>(
            name, handler, priority, policy, period, deadline, 4096, recoverable);

        tasks_[name] = task;
        scheduler_->addTask(task);

        std::cout << "Created task '" << name << "' with priority " << static_cast<int>(priority) << "\n";
        return task;
    }

    void Kernel::removeTask(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(kernel_mutex_);

        auto it = tasks_.find(name);
        if (it == tasks_.end())
        {
            std::cerr << "Task '" << name << "' not found\n";
            return;
        }

        scheduler_->removeTask(name);
        tasks_.erase(it);

        std::cout << "Removed task '" << name << "'\n";
    }

    TaskPtr Kernel::getTask(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(kernel_mutex_);

        auto it = tasks_.find(name);
        if (it == tasks_.end())
        {
            return nullptr;
        }

        return it->second;
    }

    void Kernel::suspendTask(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(kernel_mutex_);

        auto task = getTask(name);
        if (task)
        {
            task->suspend();
            std::cout << "Suspended task '" << name << "'\n";
        }
        else
        {
            std::cerr << "Task '" << name << "' not found\n";
        }
    }

    void Kernel::resumeTask(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(kernel_mutex_);

        auto task = getTask(name);
        if (task)
        {
            task->resume();
            std::cout << "Resumed task '" << name << "'\n";
        }
        else
        {
            std::cerr << "Task '" << name << "' not found\n";
        }
    }

    void Kernel::enableAutoVisualization(bool enable, std::chrono::milliseconds interval)
    {
        std::lock_guard<std::mutex> lock(kernel_mutex_);
        auto_visualization_ = enable;
        visualization_interval_ = interval;
    }

    void Kernel::visualizeTaskStates()
    {
        scheduler_->printTaskStates();
    }

    void Kernel::visualizationLoop()
    {
        while (auto_visualization_)
        {
            std::this_thread::sleep_for(visualization_interval_);
            visualizeTaskStates();
        }
    }

} // namespace edurtos
