cmake_minimum_required(VERSION 3.25)
project(PDFMaestro VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)
find_package(Poppler REQUIRED COMPONENTS Qt6)
find_package(PkgConfig REQUIRED)
pkg_check_modules(QPDF REQUIRED libqpdf)

# Optional KDE Frameworks — used when available, falls back to plain Qt
find_package(ECM QUIET)
if(ECM_FOUND)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
    find_package(KF6 QUIET COMPONENTS CoreAddons WidgetsAddons ConfigWidgets XmlGui)
endif()

set(SOURCES
    src/main.cpp
    src/config.cpp
    src/toc.cpp
    src/annotations.cpp
    src/operations.cpp
    src/pdfviewer.cpp
    src/annotationoverlay.cpp
    src/pagemanager.cpp
    src/searchbar.cpp
    src/signaturedialog.cpp
    src/settingsdialog.cpp
    src/dialogs.cpp
    src/mainwindow.cpp
)

add_executable(pdfmaestro ${SOURCES})

target_include_directories(pdfmaestro PRIVATE src ${QPDF_INCLUDE_DIRS})

target_link_libraries(pdfmaestro
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Poppler::Qt6
    ${QPDF_LIBRARIES}
)

if(KF6CoreAddons_FOUND)
    target_link_libraries(pdfmaestro
        KF6::CoreAddons
        KF6::WidgetsAddons
        KF6::ConfigWidgets
    )
    target_compile_definitions(pdfmaestro PRIVATE HAVE_KF6=1)
    message(STATUS "KDE Frameworks 6 found — enabling KDE integration")
else()
    message(STATUS "KDE Frameworks 6 not found — using plain Qt6")
endif()

install(TARGETS pdfmaestro DESTINATION bin)
install(FILES data/icons/pdfmaestro.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME pdfmaestro.svg)
