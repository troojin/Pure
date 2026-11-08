add_executable(basic main.cpp)
set_target_properties(basic PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/basic")
target_link_libraries(basic PRIVATE Pure::Pure)
Pure_copy_runtime(basic)

add_custom_command(TARGET basic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:basic>/web"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/web"
        "$<TARGET_FILE_DIR:basic>/web"
)
set_property(TARGET basic PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:basic>")
