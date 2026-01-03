# Game Application
# Standalone game executable using the engine

add_executable(game
    main.cpp
)

target_compile_features(game PRIVATE cxx_std_26)

target_link_libraries(game
    PRIVATE
        klingon
        imgui
)

set_target_properties(game PROPERTIES
    OUTPUT_NAME "game"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy DLLs to output directory on Windows
if(WIN32)
    add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:game>
            $<TARGET_FILE_DIR:game>
        COMMAND_EXPAND_LISTS
    )
endif()
