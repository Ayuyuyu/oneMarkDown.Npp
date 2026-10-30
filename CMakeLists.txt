cmake_minimum_required(VERSION 3.20)
project(OneMarkDownNpp VERSION 0.1.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT: no VC++ redistributable required on the target machine.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(TP ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# ---------------------------------------------------------------------------
# WebView2 SDK is fetched at configure time from NuGet instead of being vendored,
# so the repository only keeps small sources (md4c + Notepad++ headers).
#
#   WEBVIEW2_SDK_DIR   - use an already extracted SDK and skip the download
#   WEBVIEW2_VERSION   - NuGet package version
#   WEBVIEW2_NUGET_URL - override for a mirror / offline cache
# ---------------------------------------------------------------------------
set(WEBVIEW2_VERSION "1.0.4314.35" CACHE STRING "Microsoft.Web.WebView2 NuGet package version")
set(WEBVIEW2_NUGET_URL
    "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/${WEBVIEW2_VERSION}/microsoft.web.webview2.${WEBVIEW2_VERSION}.nupkg"
    CACHE STRING "WebView2 SDK download URL")
set(WEBVIEW2_SDK_DIR "" CACHE PATH "Path to an extracted WebView2 SDK (skips download)")

if(WEBVIEW2_SDK_DIR)
    set(WEBVIEW2_ROOT "${WEBVIEW2_SDK_DIR}")
else()
    set(WEBVIEW2_ROOT "${CMAKE_BINARY_DIR}/webview2")
endif()

set(WEBVIEW2_INCLUDE "${WEBVIEW2_ROOT}/build/native/include")
set(WEBVIEW2_LIB "${WEBVIEW2_ROOT}/build/native/x64/WebView2LoaderStatic.lib")

if(NOT EXISTS "${WEBVIEW2_LIB}")
    if(WEBVIEW2_SDK_DIR)
        message(FATAL_ERROR "WEBVIEW2_SDK_DIR does not contain build/native/x64/WebView2LoaderStatic.lib")
    endif()

    set(WEBVIEW2_ARCHIVE "${CMAKE_BINARY_DIR}/webview2-${WEBVIEW2_VERSION}.nupkg")
    message(STATUS "Downloading WebView2 SDK ${WEBVIEW2_VERSION} ...")
    file(DOWNLOAD "${WEBVIEW2_NUGET_URL}" "${WEBVIEW2_ARCHIVE}"
         STATUS download_status
         TLS_VERIFY ON
         SHOW_PROGRESS)

    list(GET download_status 0 download_code)
    if(NOT download_code EQUAL 0)
        list(GET download_status 1 download_message)
        file(REMOVE "${WEBVIEW2_ARCHIVE}")
        message(FATAL_ERROR
            "Failed to download the WebView2 SDK.\n"
            "  URL: ${WEBVIEW2_NUGET_URL}\n"
            "  ${download_message}\n"
            "Download the NuGet package manually, extract it, and configure with\n"
            "  -DWEBVIEW2_SDK_DIR=<extracted folder>")
    endif()

    # A .nupkg is a zip; libarchive decides by extension, so rename before extracting.
    set(WEBVIEW2_ARCHIVE_ZIP "${CMAKE_BINARY_DIR}/webview2-${WEBVIEW2_VERSION}.zip")
    file(RENAME "${WEBVIEW2_ARCHIVE}" "${WEBVIEW2_ARCHIVE_ZIP}")

    file(MAKE_DIRECTORY "${WEBVIEW2_ROOT}")
    file(ARCHIVE_EXTRACT INPUT "${WEBVIEW2_ARCHIVE_ZIP}" DESTINATION "${WEBVIEW2_ROOT}")
    file(REMOVE "${WEBVIEW2_ARCHIVE_ZIP}")
endif()

if(NOT EXISTS "${WEBVIEW2_LIB}")
    message(FATAL_ERROR "WebView2LoaderStatic.lib not found in ${WEBVIEW2_ROOT}")
endif()

add_library(md4c STATIC
    ${TP}/md4c/md4c.c
    ${TP}/md4c/md4c-html.c
    ${TP}/md4c/entity.c
)
target_include_directories(md4c PUBLIC ${TP}/md4c)

add_library(OneMarkDownNpp SHARED
    src/PluginDefinition.cpp
    src/PreviewPanel.cpp
    src/MarkdownRenderer.cpp
    src/ClipboardHelper.cpp
    src/PluginUtil.cpp
)

target_include_directories(OneMarkDownNpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${TP}/npp
    ${WEBVIEW2_INCLUDE}
)

target_compile_definitions(OneMarkDownNpp PRIVATE
    UNICODE _UNICODE
    WIN32_LEAN_AND_MEAN NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

if(MSVC)
    target_compile_options(OneMarkDownNpp PRIVATE /utf-8 /W3 /permissive- /MP)
    target_compile_options(md4c PRIVATE /W0)
endif()

target_link_libraries(OneMarkDownNpp PRIVATE
    md4c
    ${WEBVIEW2_LIB}
    user32 ole32 oleaut32 shell32 shlwapi advapi32 version
)

# Output into bin.
set_target_properties(OneMarkDownNpp PROPERTIES
    OUTPUT_NAME "OneMarkDownNpp"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

# Renderer smoke test.
add_executable(md_render_test tests/render_test.cpp src/MarkdownRenderer.cpp)
target_include_directories(md_render_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(md_render_test PRIVATE md4c)
if(MSVC)
    target_compile_options(md_render_test PRIVATE /utf-8 /W3)
endif()
set_target_properties(md_render_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/bin
)
