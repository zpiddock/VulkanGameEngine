# Editor Application
# Development editor with ImGui interface and hot-reload support

add_executable(editor
    main.cpp
)

target_compile_features(editor PRIVATE cxx_std_26)

target_link_libraries(editor
    PRIVATE
        klingon
        imgui
)

set_target_properties(editor PROPERTIES
    OUTPUT_NAME "editor"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy DLLs to output directory on Windows
if(WIN32)
    add_custom_command(TARGET editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:editor>
            $<TARGET_FILE_DIR:editor>
        COMMAND_EXPAND_LISTS
    )
endif()
