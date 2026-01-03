# Batleth - Vulkan Abstractions
# Provides modern RAII wrappers around Vulkan resources

add_library(batleth SHARED
    src/instance.cpp
    src/device.cpp
    src/swapchain.cpp
    src/render_pass.cpp
    src/pipeline.cpp
    src/framebuffer.cpp
    src/command_buffer.cpp
)

target_include_directories(batleth
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(batleth PUBLIC cxx_std_26)

target_link_libraries(batleth
    PUBLIC
        federation
        Vulkan::Vulkan
        VulkanMemoryAllocator
)

# Set output name and versioning
set_target_properties(batleth PROPERTIES
    OUTPUT_NAME "batleth"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Windows DLL export/import
if(WIN32)
    target_compile_definitions(batleth
        PRIVATE BATLETH_EXPORTS
        INTERFACE BATLETH_IMPORTS
    )
endif()
