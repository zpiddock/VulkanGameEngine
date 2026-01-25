# Klingon - Engine
# High-level engine integrating all subsystems

add_library(klingon SHARED
        src/engine.cpp
        src/renderer.cpp
        src/imgui_context.cpp
        src/camera.cpp
        src/transform.cpp
        src/movement_controller.cpp
        src/model/mesh.cpp
        src/game_object.cpp
        src/picking/ray_picker.cpp
        src/render_systems/simple_render_system.cpp
        src/render_systems/point_light_system.cpp
        src/render_systems/blit_render_system.cpp
        src/render_systems/depth_prepass_system.cpp
        src/render_graph.cpp
        src/scene.cpp
        src/model/asset_loader.cpp
        src/model_data.cpp
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
        imgui
        tinyobjloader
        assimp
        stb
)

# Set output name and versioning
set_target_properties(klingon PROPERTIES
        OUTPUT_NAME "klingon"
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Windows DLL export/import
if (WIN32)
    target_compile_definitions(klingon
            PRIVATE KLINGON_EXPORTS
            INTERFACE KLINGON_IMPORTS
    )
endif ()
