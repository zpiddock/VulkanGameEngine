# Klingon - Engine
# High-level engine integrating all subsystems

add_library(klingon SHARED
    src/engine.cpp
    src/renderer.cpp
)

target_include_directories(klingon
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(klingon PUBLIC cxx_std_26)

target_link_libraries(klingon
    PUBLIC
        federation
        borg
        batleth
        glm
)

# Set output name and versioning
set_target_properties(klingon PROPERTIES
    OUTPUT_NAME "klingon"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Windows DLL export/import
if(WIN32)
    target_compile_definitions(klingon
        PRIVATE KLINGON_EXPORTS
        INTERFACE KLINGON_IMPORTS
    )
endif()
