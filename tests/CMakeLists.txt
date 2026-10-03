# ---------------------------------------------------------------------------
# The tests.
#
# One executable rather than one per file: the framework is a few hundred lines
# and each test is a function, so a single binary keeps the build and the CI
# output simple. What matters is that the core builds and runs with no window,
# which is what lets these run on a Linux runner.
# ---------------------------------------------------------------------------

file(GLOB KEYFLOW_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(keyflow_tests ${KEYFLOW_TEST_SOURCES})

target_include_directories(keyflow_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(keyflow_tests PRIVATE keyflow_core)

if(TARGET keyflow_bridge)
    target_link_libraries(keyflow_tests PRIVATE keyflow_bridge)
endif()

if(TARGET keyflow_media)
    target_link_libraries(keyflow_tests PRIVATE keyflow_media)
endif()

target_compile_options(keyflow_tests PRIVATE ${KEYFLOW_WARNINGS})

if(MSVC)
    # 4127: "conditional expression is constant". The pinned-value tests exist
    # precisely to compare compile-time constants, so the warning is the test
    # working as intended rather than a defect. Suppressed here and nowhere
    # else.
    target_compile_options(keyflow_tests PRIVATE /wd4127)
endif()

add_test(NAME keyflow_tests COMMAND keyflow_tests)
