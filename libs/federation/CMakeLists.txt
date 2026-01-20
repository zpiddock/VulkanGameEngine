# Federation - Core library
# Provides logging, configuration handling, and other core utilities

add_library(federation SHARED
        src/core.cpp
        src/log.cpp
        src/config_manager.cpp
)

target_include_directories(federation
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(federation PUBLIC cxx_std_26)

# Link Cereal for JSON serialization (header-only)
target_link_libraries(federation PUBLIC ser20)

# Set output name and versioning
set_target_properties(federation PROPERTIES
        OUTPUT_NAME "federation"
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Windows DLL export/import
if (WIN32)
    target_compile_definitions(federation
            PRIVATE FEDERATION_EXPORTS
            INTERFACE FEDERATION_IMPORTS
    )
endif ()
