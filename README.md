# Klingon Engine

A modern Vulkan-based 3D game engine written in C++26 with hot-reload support for rapid development.

## Architecture

The engine is structured into modular libraries:

- **Federation** - Core library providing logging, configuration handling, and foundational utilities
- **Borg** - Application framework with GLFW abstractions for windowing and input handling
- **Batleth** - Vulkan abstractions using modern RAII techniques
- **Klingon** - High-level engine integrating all subsystems

## Applications

- **Game** - Standalone game executable with ImGui debug interface
- **Editor** - Development editor with full ImGui interface and hot-reload support

## Features

- Modern C++26 with heavy use of `auto` keyword and RAII patterns
- Vulkan 1.3 graphics API
- Hot-reload support for shaders and game code
- Dynamic library architecture for fast iteration
- CMake-based build system with FetchContent dependency management
- Warnings treated as errors for code quality

## Dependencies

All dependencies are automatically fetched via CMake FetchContent:

- [GLFW](https://github.com/glfw/glfw) - Windowing and input
- [GLM](https://github.com/g-truc/glm) - Mathematics library
- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI
- [Vulkan SDK](https://vulkan.lunarg.com/) - Graphics API (must be installed separately)
- [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) - Memory management

## Requirements

- CMake 3.28 or higher
- MinGW-w64 (GCC) with C++26 support
- Ninja build system
- Vulkan SDK 1.3 or higher

## Building

The project uses a Makefile for convenient build commands:

```bash
# Configure the project
make configure

# Build debug configuration
make build-debug

# Build release configuration
make build-release

# Build both configurations
make build-all

# Clean build directory
make clean
```

## Running

```bash
# Run the game application
make run-game

# Run the editor application
make run-editor
```

## Project Structure

```
VulkanTake2/
├── cmake/
│   └── dependencies.cmake      # FetchContent dependency management
├── src/
│   ├── federation/            # Core library
│   │   ├── include/federation/
│   │   └── src/
│   ├── borg/                  # Application framework
│   │   ├── include/borg/
│   │   └── src/
│   ├── batleth/               # Vulkan abstractions
│   │   ├── include/batleth/
│   │   └── src/
│   ├── klingon/               # Engine
│   │   ├── include/klingon/
│   │   └── src/
│   ├── game/                  # Game executable
│   │   └── main.cpp
│   └── editor/                # Editor executable
│       └── main.cpp
├── CMakeLists.txt
├── Makefile
├── LICENSE
└── README.md
```

## Naming Conventions

- Classes: PascalCase (e.g., `Engine`, `Window`)
- Files: snake_case (e.g., `engine.hpp`, `window.cpp`)
- Namespaces: lowercase matching library name (e.g., `klingon`, `borg`)

## Code Style

- Modern C++26 features encouraged
- RAII patterns for resource management
- Heavy use of `auto` keyword where it improves readability
- Global namespace functions prefixed with `::` (e.g., `::vkCreateInstance`)
- Comments used where code is not self-documenting

## License

The libraries (Federation, Borg, Batleth, Klingon) are licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.

## Development Status

This project is in early development. Current features:

- [x] Project structure and build system
- [x] Basic window management
- [x] Vulkan instance and device abstraction
- [ ] Vulkan rendering pipeline
- [ ] Shader hot-reload system
- [ ] ImGui integration
- [ ] Resource management
- [ ] Scene graph
- [ ] Entity component system

## Contributing

This is a personal project, but suggestions and feedback are welcome!
