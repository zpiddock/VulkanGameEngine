# Demo Game Application
# Demonstrates the Klingon Engine capabilities

add_executable(game main.cpp)

target_link_libraries(game
        PRIVATE
        klingon
)

# Copy DLLs to output directory (Windows)
if (WIN32)
    add_custom_command(TARGET game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:game>
            $<TARGET_FILE_DIR:game>
            COMMAND_EXPAND_LISTS
    )
endif ()
