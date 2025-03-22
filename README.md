# EDURTOS - Educational Real-Time Operating System

EDURTOS is a minimal, educational real-time operating system designed for learning the fundamentals of RTOS concepts and embedded systems programming.

## Overview

This project implements a simple RTOS with basic features commonly found in commercial RTOSes, but focused on clarity and learning rather than performance or completeness. It's ideal for students and hobbyists looking to understand how real-time operating systems function internally.

## Features

- Task scheduling with multiple priorities
- Context switching between tasks
- Basic task synchronization primitives
- Memory management
- Time management capabilities
- Simple API for application development

## Getting Started

### Prerequisites

- GCC-compatible toolchain
- Make build system
- Your target embedded platform or simulator

### Building the Project

```bash
make
```

### Running Examples

```bash
make run-example
```

## Project Structure

- `src/` - Core RTOS source files
- `include/` - Header files
- `examples/` - Example applications
- `docs/` - Documentation

## API Documentation

See the `docs/` directory for detailed API documentation.

## Contributing

Contributions to improve EDURTOS are welcome. Please feel free to submit pull requests or open issues for bugs and feature requests.

## License

This project is available under [LICENSE] terms.

## Acknowledgments

This educational RTOS was developed as part of coursework for Operating Systems at [Your University/College Name].
