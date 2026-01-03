# Makefile for Vulkan Game Engine
# Provides convenient build targets for Windows (MinGW) with Ninja Multi-Config

.PHONY: help configure build-debug build-release build-all run-game run-editor clean rebuild-debug rebuild-release test

# Default target
help:
	@echo "Available targets:"
	@echo "  configure      - Configure CMake project with Ninja Multi-Config"
	@echo "  build-debug    - Build debug configuration"
	@echo "  build-release  - Build release configuration"
	@echo "  build-all      - Build both debug and release configurations"
	@echo "  run-game       - Run the game application (debug)"
	@echo "  run-editor     - Run the editor application (debug)"
	@echo "  rebuild-debug  - Clean and rebuild debug configuration"
	@echo "  rebuild-release- Clean and rebuild release configuration"
	@echo "  clean          - Remove build directory"
	@echo "  test           - Run tests (when implemented)"

# Configure the project
configure:
	cmake -B build -G "Ninja Multi-Config" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++

# Build debug configuration
build-debug: configure
	cmake --build build --config Debug

# Build release configuration
build-release: configure
	cmake --build build --config Release

# Build all configurations
build-all: build-debug build-release

# Run game in debug mode
run-game: build-debug
	@echo "Launching Game..."
	@cd build/bin && "./Debug/game.exe"

# Run editor in debug mode
run-editor: build-debug
	@echo "Launching Editor..."
	@cd build/bin && "./Debug/editor.exe"

# Clean build directory
clean:
	@echo "Cleaning build directory..."
	@if exist build rmdir /s /q build

# Rebuild debug from scratch
rebuild-debug: clean build-debug

# Rebuild release from scratch
rebuild-release: clean build-release

# Run tests (placeholder)
test:
	@echo "Tests not yet implemented"
	@cmake --build build --config Debug --target test
