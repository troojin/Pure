cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(Pure VERSION 0.2.0 LANGUAGES CXX)
endif()

if(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Pure requires a 64-bit build on Windows.")
    endif()

    include(FetchContent)

    set(PURE_WEBVIEW2_VERSION "1.0.2478.35")
    FetchContent_Declare(webview2
        URL "https://globalcdn.nuget.org/packages/microsoft.web.webview2.${PURE_WEBVIEW2_VERSION}.nupkg"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(webview2)

    set(PURE_WEBVIEW2_ROOT "${webview2_SOURCE_DIR}/build/native")
    if(NOT EXISTS "${PURE_WEBVIEW2_ROOT}/include/WebView2.h")
        message(FATAL_ERROR "WebView2 SDK download did not contain WebView2.h.")
    endif()

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(PURE_WEBVIEW2_ARCH "arm64")
    else()
        set(PURE_WEBVIEW2_ARCH "x64")
    endif()

    add_library(Pure STATIC
        src/core/Context.cpp
        src/platform/win32/Win32Backend.cpp
    )

    target_include_directories(Pure
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${PURE_WEBVIEW2_ROOT}/include
    )

    target_link_libraries(Pure PRIVATE
        "${PURE_WEBVIEW2_ROOT}/${PURE_WEBVIEW2_ARCH}/WebView2Loader.dll.lib"
        comctl32
        ole32
        oleaut32
        dwmapi
        user32
        gdi32
        shell32
    )

    target_compile_definitions(Pure PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

    set(PURE_WEBVIEW2_DLL "${PURE_WEBVIEW2_ROOT}/${PURE_WEBVIEW2_ARCH}/WebView2Loader.dll" CACHE INTERNAL "")

    function(Pure_copy_runtime target)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PURE_WEBVIEW2_DLL}"
                "$<TARGET_FILE_DIR:${target}>"
            VERBATIM
        )
    endfunction()

elseif(APPLE)
    enable_language(OBJCXX)

    add_library(Pure STATIC
        src/core/Context.cpp
        src/platform/macos/MacOSBackend.mm
    )

    set_source_files_properties(src/platform/macos/MacOSBackend.mm PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc"
    )

    target_include_directories(Pure
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(Pure PRIVATE
        "-framework Cocoa"
        "-framework WebKit"
    )

    function(Pure_copy_runtime target)
    endfunction()

elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
    pkg_check_modules(WEBKIT2 QUIET IMPORTED_TARGET webkit2gtk-4.1)
    if(NOT WEBKIT2_FOUND)
        pkg_check_modules(WEBKIT2 REQUIRED IMPORTED_TARGET webkit2gtk-4.0)
    endif()

    add_library(Pure STATIC
        src/core/Context.cpp
        src/platform/linux/LinuxBackend.cpp
    )

    target_include_directories(Pure
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(Pure PRIVATE
        PkgConfig::GTK3
        PkgConfig::WEBKIT2
    )

    function(Pure_copy_runtime target)
    endfunction()

else()
    message(FATAL_ERROR "Unsupported platform.")
endif()

add_library(Pure::Pure ALIAS Pure)
target_compile_features(Pure PUBLIC cxx_std_17)
