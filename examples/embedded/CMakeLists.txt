if(WIN32)
    add_executable(embedded WIN32 main.cpp)
    target_link_libraries(embedded PRIVATE dwmapi)
else()
    add_executable(embedded main.cpp)
endif()

set_target_properties(embedded PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/embedded")
target_link_libraries(embedded PRIVATE Pure::Pure)

if(APPLE)
    target_link_libraries(embedded PRIVATE "-framework Cocoa")
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
    target_link_libraries(embedded PRIVATE PkgConfig::GTK3)
endif()

Pure_copy_runtime(embedded)

add_custom_command(TARGET embedded POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:embedded>/web"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/web"
        "$<TARGET_FILE_DIR:embedded>/web"
)
set_property(TARGET embedded PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:embedded>")
