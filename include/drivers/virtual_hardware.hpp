#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <atomic>
#include <functional>

namespace edurtos
{
    namespace drivers
    {

        // Virtual GPIO Device
        class VirtualGPIO
        {
        public:
            static constexpr std::size_t PIN_COUNT = 16;

            enum class PinMode
            {
                INPUT,
                OUTPUT,
                INPUT_PULLUP,
                INPUT_PULLDOWN
            };

            VirtualGPIO();
            void setPinMode(std::uint8_t pin, PinMode mode);
            void writePin(std::uint8_t pin, bool value);
            bool readPin(std::uint8_t pin) const;
            void registerInterrupt(std::uint8_t pin, std::function<void()> handler);

        private:
            std::array<PinMode, PIN_COUNT> pin_modes_;
            std::array<bool, PIN_COUNT> pin_states_;
            std::array<std::function<void()>, PIN_COUNT> interrupt_handlers_;
        };

        // Virtual Timer Device
        class VirtualTimer
        {
        public:
            enum class TimerMode
            {
                ONE_SHOT,
                PERIODIC
            };

            VirtualTimer();
            void start(std::uint32_t interval_ms, TimerMode mode);
            void stop();
            bool isRunning() const;
            void registerCallback(std::function<void()> callback);
            void update(); // Should be called periodically by the scheduler

        private:
            std::atomic<bool> running_{false};
            std::uint32_t interval_ms_{0};
            TimerMode mode_{TimerMode::ONE_SHOT};
            std::function<void()> callback_;
            std::uint64_t last_trigger_time_{0};
        };

        // Virtual UART Device
        class VirtualUART
        {
        public:
            enum class BaudRate
            {
                BAUD_9600,
                BAUD_19200,
                BAUD_38400,
                BAUD_57600,
                BAUD_115200
            };

            VirtualUART();
            void configure(BaudRate baud_rate);
            void transmit(const std::string &data);
            std::string receive();
            bool hasData() const;

        private:
            BaudRate baud_rate_{BaudRate::BAUD_115200};
            std::string receive_buffer_;
        };

        // Hardware abstraction layer that collects all virtual devices
        class HAL
        {
        public:
            static HAL &getInstance();

            VirtualGPIO &getGPIO();
            VirtualTimer &getTimer();
            VirtualUART &getUART();

        private:
            HAL();
            ~HAL() = default;
            HAL(const HAL &) = delete;
            HAL &operator=(const HAL &) = delete;

            VirtualGPIO gpio_;
            VirtualTimer timer_;
            VirtualUART uart_;
        };

    } // namespace drivers
} // namespace edurtos
