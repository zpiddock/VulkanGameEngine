cmake_minimum_required(VERSION 4.2)

project(VulkanGameEngine
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "A modern Vulkan-based game engine with hot-reload support"
)

# Set C++26 standard
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Treat warnings as errors
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter)
endif()

# Disable logging for Release and RelWithDebInfo builds
if(CMAKE_BUILD_TYPE MATCHES "Release" OR CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
    add_compile_definitions(FEDERATION_DISABLE_LOGGING)
    message(STATUS "Logging disabled for ${CMAKE_BUILD_TYPE} build")
endif()

# Include dependencies
include(cmake/dependencies.cmake)

message(STATUS "Vulkan include: ${Vulkan_INCLUDE_DIRS}")

# Add subdirectories for libraries
add_subdirectory(libs/federation)
add_subdirectory(libs/borg)
add_subdirectory(libs/batleth)

# Add engine
add_subdirectory(engine/klingon)

# Add applications
add_subdirectory(apps/demo-game)
add_subdirectory(apps/editor)
