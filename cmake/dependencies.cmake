# Dependencies management using FetchContent

include(FetchContent)

# Set FetchContent options
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

# SPIRV-Headers - Required by SPIRV-Tools
FetchContent_Declare(
    spirv-headers
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Headers.git
    GIT_TAG vulkan-sdk-1.3.290.0
    GIT_SHALLOW TRUE
)

# SPIRV-Tools - Required by GLSLang for optimization
FetchContent_Declare(
    spirv-tools
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Tools.git
    GIT_TAG vulkan-sdk-1.3.290.0
    GIT_SHALLOW TRUE
)

set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SPIRV_WERROR OFF CACHE BOOL "" FORCE)

# GLSLang - GLSL to SPIR-V compiler (built from source for ABI compatibility)
FetchContent_Declare(
    glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG 15.0.0
    GIT_SHALLOW TRUE
)

# Configure glslang build options
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_JS OFF CACHE BOOL "" FORCE)
set(ENABLE_RTTI ON CACHE BOOL "" FORCE)
set(ENABLE_EXCEPTIONS ON CACHE BOOL "" FORCE)
set(ENABLE_OPT ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(spirv-headers spirv-tools glslang)

# Disable warnings-as-errors for third-party libraries
if(TARGET glslang)
    target_compile_options(glslang PRIVATE -Wno-error)
endif()
if(TARGET SPIRV)
    target_compile_options(SPIRV PRIVATE -w)
endif()
if(TARGET SPIRV-Tools-static)
    target_compile_options(SPIRV-Tools-static PRIVATE -w)
endif()
if(TARGET SPIRV-Tools-opt)
    target_compile_options(SPIRV-Tools-opt PRIVATE -w)
endif()

# GLFW - Windowing library
FetchContent_Declare(
        glfw
        URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
        FIND_PACKAGE_ARGS 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_LIBRARY_TYPE SHARED CACHE STRING "" FORCE)

# GLM - Mathematics library
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
    GIT_SHALLOW TRUE
)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)

# Dear ImGui - UI library (docking branch)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.5-docking
    GIT_SHALLOW TRUE
)

# VulkanMemoryAllocator
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.3.0
    GIT_SHALLOW TRUE
)

# TinyObjLoader - Simple OBJ file loader
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13
    GIT_SHALLOW TRUE
)

# Assimp
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

# Example: disable formats you don't need
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v6.0.2   # use a stable release tag
        GIT_SHALLOW TRUE
)

# Cereal - C++ serialization library
FetchContent_Declare(
    cereal
    GIT_REPOSITORY https://github.com/USCiLab/cereal.git
    GIT_TAG v1.3.2
    GIT_SHALLOW TRUE
)

# Cereal is header-only, no need for special configuration
set(JUST_INSTALL_CEREAL ON CACHE BOOL "" FORCE)
set(SKIP_PERFORMANCE_COMPARISON ON CACHE BOOL "" FORCE)

# Make dependencies available
FetchContent_MakeAvailable(glfw glm vma imgui tinyobjloader assimp cereal)

# Disable warnings for third-party libraries
if(TARGET glfw)
    target_compile_options(glfw PRIVATE -w)
endif()

# VMA is header-only, so we disable warnings via INTERFACE
if(TARGET VulkanMemoryAllocator)
    target_compile_options(VulkanMemoryAllocator INTERFACE -w)
endif()

# ImGui needs special handling as it doesn't have CMakeLists.txt
# Create ImGui library target as SHARED (DLL) to avoid context issues across modules
add_library(imgui SHARED
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC
    glfw
    Vulkan::Vulkan
)

# Export ImGui symbols on Windows
if(WIN32)
    target_compile_definitions(imgui
        PRIVATE IMGUI_API=__declspec\(dllexport\)
    )
    target_compile_definitions(imgui
        INTERFACE IMGUI_API=__declspec\(dllimport\)
    )
endif()

# Disable warnings for ImGui
if(MSVC)
    target_compile_options(imgui PRIVATE /W0)
else()
    target_compile_options(imgui PRIVATE -w)
endif()
