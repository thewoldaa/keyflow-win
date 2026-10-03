# ---------------------------------------------------------------------------
# Sources are globbed rather than listed.
#
# Deliberate, and the reason is the worktree harness: one worktree per task,
# and an explicit source list is a file every task must edit. Every task
# editing the same file means every task conflicts with every other task,
# which is exactly what the harness exists to prevent. Adding a source file
# inside a task's own territory must not require touching a shared file.
#
# CONFIGURE_DEPENDS makes the build re-run CMake when a file is added or
# removed, so the glob does not go stale.
# ---------------------------------------------------------------------------

# --- core ------------------------------------------------------------------
#
# No Windows headers, no WebView2, no OpenGL. Builds and runs in a test process
# with no window and no message loop. This is what makes the model, the project
# format, the effect catalogue and the expression evaluator testable, and what
# lets the parallel tasks in later waves be genuinely independent.

file(
    GLOB_RECURSE KEYFLOW_CORE_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/gl/ShaderTable.cpp"
)

add_library(keyflow_core STATIC ${KEYFLOW_CORE_SOURCES})

target_include_directories(keyflow_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_options(keyflow_core PRIVATE ${KEYFLOW_WARNINGS})

# ---------------------------------------------------------------------------
# bridge
#
# The message protocol between the page and the host. Free of WebView2 types
# on purpose: the host calls HandleMessage with text that arrived from the web
# view and the bridge calls back with text to send, so the protocol can be
# tested without a window. It links the core and nothing platform-specific,
# which is why it builds on every platform even though the editor it serves is
# Windows-only.
# ---------------------------------------------------------------------------

file(
    GLOB KEYFLOW_BRIDGE_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/app/Bridge.cpp"
)

if(KEYFLOW_BRIDGE_SOURCES)
    add_library(keyflow_bridge STATIC ${KEYFLOW_BRIDGE_SOURCES})
    target_include_directories(keyflow_bridge PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(keyflow_bridge PUBLIC keyflow_core)
    target_compile_options(keyflow_bridge PRIVATE ${KEYFLOW_WARNINGS})
endif()

# ---------------------------------------------------------------------------
# media
#
# Decode and encode. Shells out to ffmpeg rather than linking libav: the
# runtime dependency is one executable the user already has, the build has no
# third-party link step, and a decoder crash cannot take the editor with it.
# See docs/ARCHITECTURE.md.
# ---------------------------------------------------------------------------

file(
    GLOB_RECURSE KEYFLOW_MEDIA_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/media/*.cpp"
)

if(KEYFLOW_MEDIA_SOURCES)
    add_library(keyflow_media STATIC ${KEYFLOW_MEDIA_SOURCES})
    target_include_directories(keyflow_media PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(keyflow_media PUBLIC keyflow_core)
    target_compile_options(keyflow_media PRIVATE ${KEYFLOW_WARNINGS})
endif()

# ---------------------------------------------------------------------------
# platform
#
# Win32 only: file dialogs, DPI, window chrome. Isolated so that core never
# needs a Windows header.
# ---------------------------------------------------------------------------

if(WIN32)
    file(
        GLOB_RECURSE KEYFLOW_PLATFORM_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.cpp"
    )

    if(KEYFLOW_PLATFORM_SOURCES)
        add_library(keyflow_platform STATIC ${KEYFLOW_PLATFORM_SOURCES})
        target_include_directories(keyflow_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
        target_link_libraries(keyflow_platform PUBLIC keyflow_core)
        target_compile_options(keyflow_platform PRIVATE ${KEYFLOW_WARNINGS})
        target_link_libraries(keyflow_platform PRIVATE comdlg32 shell32 ole32)
    endif()
endif()

# ---------------------------------------------------------------------------
# renderer
#
# The OpenGL compositor. Win32 only because it creates a hidden window and an
# offscreen GL context; the shader table it compiles lives in core so the
# tests can reach it on any platform.
# ---------------------------------------------------------------------------

if(WIN32)
    file(
        GLOB_RECURSE KEYFLOW_RENDER_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/gl/*.cpp"
    )
    # ShaderTable.cpp is in core; everything else under gl/ is the renderer.
    list(REMOVE_ITEM KEYFLOW_RENDER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gl/ShaderTable.cpp")

    if(KEYFLOW_RENDER_SOURCES)
        add_library(keyflow_render STATIC ${KEYFLOW_RENDER_SOURCES})
        target_include_directories(keyflow_render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
        target_link_libraries(keyflow_render PUBLIC keyflow_core keyflow_media)
        target_compile_options(keyflow_render PRIVATE ${KEYFLOW_WARNINGS})
        target_link_libraries(keyflow_render PRIVATE opengl32 gdi32 user32)
    endif()
endif()

# ---------------------------------------------------------------------------
# application
# ---------------------------------------------------------------------------

if(WIN32 AND KEYFLOW_BUILD_APP AND KEYFLOW_HAVE_WEBVIEW2)
    file(
        GLOB_RECURSE KEYFLOW_APP_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp"
    )

    add_executable(keyflow WIN32 ${KEYFLOW_APP_SOURCES})

    target_include_directories(
        keyflow
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/ui/assets"
            "${KEYFLOW_WEBVIEW2_DIR}/build/native/include"
    )

    target_link_libraries(
        keyflow
        PRIVATE
            keyflow_core
            "${KEYFLOW_WEBVIEW2_DIR}/build/native/x64/WebView2LoaderStatic.lib"
            user32
            gdi32
            ole32
            oleaut32
            shlwapi
            version
            comdlg32
            shell32
            comctl32
    )

    if(TARGET keyflow_bridge)
        target_link_libraries(keyflow PRIVATE keyflow_bridge)
    endif()

    if(TARGET keyflow_platform)
        target_link_libraries(keyflow PRIVATE keyflow_platform)
    endif()

    if(TARGET keyflow_render)
        target_link_libraries(keyflow PRIVATE keyflow_render)
    endif()

    if(TARGET keyflow_media)
        target_link_libraries(keyflow PRIVATE keyflow_media)
    endif()

    target_compile_options(keyflow PRIVATE ${KEYFLOW_WARNINGS})

    # UNICODE and _UNICODE make the Win32 macros resolve to their wide forms:
    # RT_RCDATA, IDC_ARROW and friends are MAKEINTRESOURCE, which picks the
    # ANSI or wide variant based on these defines. Without them every resource
    # lookup passes an LPSTR where an LPCWSTR is expected, and the errors point
    # at the call site rather than at the missing define.
    #
    # The project is Unicode throughout: JSON is UTF-8, the page is UTF-8, and
    # Win32 is UTF-16. Nothing here wants an ANSI code page.
    target_compile_definitions(keyflow PRIVATE UNICODE _UNICODE)

    # The HTML interface is compiled into the executable as a resource and
    # loaded with NavigateToString, so there is no temporary file to write and
    # no local HTTP server to run.
    set(KEYFLOW_UI_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/ui/assets/app.rc")
    if(EXISTS "${KEYFLOW_UI_RESOURCE}")
        target_sources(keyflow PRIVATE "${KEYFLOW_UI_RESOURCE}")

        # rc.exe reads ui.html because app.rc includes it, but the build system
        # does not know that: editing the interface alone would relink the
        # executable without recompiling the resource, and the app would keep
        # showing the previous page. The dependency is declared explicitly so
        # a change to the interface is always a change to the binary.
        set(KEYFLOW_UI_HTML "${CMAKE_CURRENT_SOURCE_DIR}/ui/assets/ui.html")
        set(KEYFLOW_UI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/ui/assets/app.ico")
        set_property(
            SOURCE "${KEYFLOW_UI_RESOURCE}"
            APPEND
            PROPERTY OBJECT_DEPENDS "${KEYFLOW_UI_HTML};${KEYFLOW_UI_ICON}"
        )
    endif()

    # The resource compiler needs to find ui.html next to app.rc.
    set_property(
        TARGET keyflow
        PROPERTY
            VS_GLOBAL_ResourceIncludePath
                "${CMAKE_CURRENT_SOURCE_DIR}/ui/assets"
    )
endif()
