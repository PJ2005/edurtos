#include "../../include/drivers/virtual_hardware.hpp"
#include <chrono>
#include <stdexcept>
#include <iostream>

namespace edurtos
{
    namespace drivers
    {

        // VirtualGPIO Implementation
        VirtualGPIO::VirtualGPIO()
        {
            // Initialize all pins to INPUT mode
            for (auto &mode : pin_modes_)
            {
                mode = PinMode::INPUT;
            }

            // Initialize all pins to LOW state
            for (auto &state : pin_states_)
            {
                state = false;
            }
        }

        void VirtualGPIO::setPinMode(std::uint8_t pin, PinMode mode)
        {
            if (pin >= PIN_COUNT)
            {
                throw std::out_of_range("Pin number out of range");
            }
            pin_modes_[pin] = mode;
        }

        void VirtualGPIO::writePin(std::uint8_t pin, bool value)
        {
            if (pin >= PIN_COUNT)
            {
                throw std::out_of_range("Pin number out of range");
            }

            if (pin_modes_[pin] != PinMode::OUTPUT)
            {
                std::cerr << "Warning: Writing to non-OUTPUT pin" << std::endl;
                return;
            }

            pin_states_[pin] = value;
            std::cout << "GPIO Pin " << static_cast<int>(pin) << " set to "
                      << (value ? "HIGH" : "LOW") << std::endl;
        }

        bool VirtualGPIO::readPin(std::uint8_t pin) const
        {
            if (pin >= PIN_COUNT)
            {
                throw std::out_of_range("Pin number out of range");
            }
            return pin_states_[pin];
        }

        void VirtualGPIO::registerInterrupt(std::uint8_t pin, std::function<void()> handler)
        {
            if (pin >= PIN_COUNT)
            {
                throw std::out_of_range("Pin number out of range");
            }
            interrupt_handlers_[pin] = std::move(handler);
        }

        // VirtualTimer Implementation
        VirtualTimer::VirtualTimer() = default;

        void VirtualTimer::start(std::uint32_t interval_ms, TimerMode mode)
        {
            interval_ms_ = interval_ms;
            mode_ = mode;
            running_ = true;
            last_trigger_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count();

            std::cout << "Virtual Timer started with interval " << interval_ms << "ms" << std::endl;
        }

        void VirtualTimer::stop()
        {
            running_ = false;
            std::cout << "Virtual Timer stopped" << std::endl;
        }

        bool VirtualTimer::isRunning() const
        {
            return running_;
        }

        void VirtualTimer::registerCallback(std::function<void()> callback)
        {
            callback_ = std::move(callback);
        }

        void VirtualTimer::update()
        {
            if (!running_ || !callback_)
            {
                return;
            }

            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();

            if (now - last_trigger_time_ >= interval_ms_)
            {
                callback_();
                last_trigger_time_ = now;

                // If one-shot timer, stop after triggering
                if (mode_ == TimerMode::ONE_SHOT)
                {
                    stop();
                }
            }
        }

        // VirtualUART Implementation
        VirtualUART::VirtualUART() = default;

        void VirtualUART::configure(BaudRate baud_rate)
        {
            baud_rate_ = baud_rate;
            std::cout << "UART configured with baud rate: ";

            switch (baud_rate)
            {
            case BaudRate::BAUD_9600:
                std::cout << "9600";
                break;
            case BaudRate::BAUD_19200:
                std::cout << "19200";
                break;
            case BaudRate::BAUD_38400:
                std::cout << "38400";
                break;
            case BaudRate::BAUD_57600:
                std::cout << "57600";
                break;
            case BaudRate::BAUD_115200:
                std::cout << "115200";
                break;
            }

            std::cout << std::endl;
        }

        void VirtualUART::transmit(const std::string &data)
        {
            std::cout << "UART TX: " << data << std::endl;
        }

        std::string VirtualUART::receive()
        {
            std::string data = receive_buffer_;
            receive_buffer_.clear();
            return data;
        }

        bool VirtualUART::hasData() const
        {
            return !receive_buffer_.empty();
        }

        // HAL Implementation
        HAL &HAL::getInstance()
        {
            static HAL instance;
            return instance;
        }

        HAL::HAL() = default;

        VirtualGPIO &HAL::getGPIO()
        {
            return gpio_;
        }

        VirtualTimer &HAL::getTimer()
        {
            return timer_;
        }

        VirtualUART &HAL::getUART()
        {
            return uart_;
        }

    } // namespace drivers
} // namespace edurtos
