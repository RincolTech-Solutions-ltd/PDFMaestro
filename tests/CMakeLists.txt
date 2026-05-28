cmake_minimum_required(VERSION 3.25)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(QPDF REQUIRED libqpdf)

set(TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${QPDF_INCLUDE_DIRS}
)

set(TEST_FLAGS
    ${QPDF_CFLAGS_OTHER}
    -DPOINTERHOLDER_TRANSITION=4
)

macro(add_pm_test NAME SOURCES)
    cmake_parse_arguments(_T "" "" "EXTRA_LIBS" ${ARGN})
    add_executable(${NAME} ${SOURCES})
    set_target_properties(${NAME} PROPERTIES AUTOMOC ON)
    target_include_directories(${NAME} PRIVATE ${TEST_INCLUDES})
    target_compile_options(${NAME} PRIVATE ${TEST_FLAGS})
    target_link_libraries(${NAME} PRIVATE
        Qt6::Core Qt6::Test
        ${_T_EXTRA_LIBS}
    )
    add_test(NAME ${NAME} COMMAND ${NAME} -v2)
    set_tests_properties(${NAME} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
    )
endmacro()

# ── tst_stamp ─────────────────────────────────────────────────────────────────
add_pm_test(tst_stamp
    "${CMAKE_CURRENT_SOURCE_DIR}/tst_stamp.cpp;${CMAKE_SOURCE_DIR}/src/annotations.cpp"
    EXTRA_LIBS Qt6::Gui ${QPDF_LIBRARIES}
)
