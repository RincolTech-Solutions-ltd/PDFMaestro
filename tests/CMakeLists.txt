cmake_minimum_required(VERSION 3.25)

# Qt6 Test + Gui components
find_package(Qt6 REQUIRED COMPONENTS Core Gui Test)

find_package(PkgConfig REQUIRED)
pkg_check_modules(QPDF REQUIRED libqpdf)
pkg_check_modules(POPPLER_QT6 REQUIRED poppler-qt6)

# ─── Shared settings ──────────────────────────────────────────────────────────
# Each test binary compiles only the source module it exercises, to keep
# test targets small and link times short.

set(TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${QPDF_INCLUDE_DIRS}
)

set(TEST_COMPILE_FLAGS
    ${QPDF_CFLAGS_OTHER}
    -DPOINTERHOLDER_TRANSITION=4
)

# ─── Helper macro ─────────────────────────────────────────────────────────────
macro(add_pdfmaestro_test NAME SOURCES)
    cmake_parse_arguments(_T "" "" "EXTRA_LIBS" ${ARGN})

    add_executable(${NAME} ${SOURCES})
    set_target_properties(${NAME} PROPERTIES AUTOMOC ON)
    target_include_directories(${NAME} PRIVATE ${TEST_INCLUDES})
    target_compile_options(${NAME} PRIVATE ${TEST_COMPILE_FLAGS})
    target_link_libraries(${NAME} PRIVATE
        Qt6::Core
        Qt6::Test
        ${_T_EXTRA_LIBS}
    )
    # -v2 gives per-test-method pass/fail lines; --output-on-failure shows them
    add_test(NAME ${NAME} COMMAND ${NAME} -v2)
    set_tests_properties(${NAME} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
    )
endmacro()

# ─── tst_operations ───────────────────────────────────────────────────────────
# operations.cpp pulls in QImage/QBuffer (Qt6::Gui) even for the tests that
# don't call overlayImageOnPage — the translation unit must compile cleanly.
add_pdfmaestro_test(tst_operations
    "${CMAKE_CURRENT_SOURCE_DIR}/tst_operations.cpp;${CMAKE_SOURCE_DIR}/src/operations.cpp"
    EXTRA_LIBS Qt6::Gui ${QPDF_LIBRARIES}
)

# ─── tst_config ───────────────────────────────────────────────────────────────
# config.cpp only needs Qt6::Core — no QPDF, no Poppler.
add_pdfmaestro_test(tst_config
    "${CMAKE_CURRENT_SOURCE_DIR}/tst_config.cpp;${CMAKE_SOURCE_DIR}/src/config.cpp"
)

# ─── tst_annotations ──────────────────────────────────────────────────────────
# annotations.h uses QColor / QPointF (Qt6::Gui).
add_pdfmaestro_test(tst_annotations
    "${CMAKE_CURRENT_SOURCE_DIR}/tst_annotations.cpp;${CMAKE_SOURCE_DIR}/src/annotations.cpp"
    EXTRA_LIBS Qt6::Gui ${QPDF_LIBRARIES}
)
