# Borg - Application Framework
# Provides GLFW abstractions like Window and Input Handling

add_library(borg SHARED
    src/window.cpp
    src/input.cpp
)

target_include_directories(borg
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(borg PUBLIC cxx_std_26)

target_link_libraries(borg
    PUBLIC
        federation
        glfw
)

# Set output name and versioning
set_target_properties(borg PROPERTIES
    OUTPUT_NAME "borg"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Windows DLL export/import
if(WIN32)
    target_compile_definitions(borg
        PRIVATE BORG_EXPORTS
        INTERFACE BORG_IMPORTS
    )
endif()
