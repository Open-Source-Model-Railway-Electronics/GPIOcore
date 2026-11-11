cmake_minimum_required(VERSION 3.16)

project(GPIOcoreGUI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8)   # sources contain UTF-8 text (arrows, degree sign)
endif()

# Works with Qt 6 (preferred) and Qt 5.12+
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort Test)

# Core logic without UI: protocol, config model, device link, simulator.
# Shared by the application and the unit tests.
add_library(gpiocore_core STATIC
    src/protocol.h        src/protocol.cpp
    src/gpioconfig.h      src/gpioconfig.cpp
    src/connections.h     src/connections.cpp
    src/devicelink.h      src/devicelink.cpp
    src/simulateddevice.h src/simulateddevice.cpp
)
target_include_directories(gpiocore_core PUBLIC src)
target_link_libraries(gpiocore_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::SerialPort)

add_library(gpiocore_ui STATIC
    src/mainwindow.h      src/mainwindow.cpp
    src/outputeditor.h    src/outputeditor.cpp
    src/inputeditor.h     src/inputeditor.cpp
    src/pinmapview.h      src/pinmapview.cpp
    src/connectionview.h  src/connectionview.cpp
    src/uihelpers.h       src/uihelpers.cpp
    src/infotexts.h       src/infotexts.cpp
    src/i18n.h            src/i18n.cpp
)
target_link_libraries(gpiocore_ui PUBLIC gpiocore_core Qt${QT_VERSION_MAJOR}::Widgets)

# Help texts: i18n/info/<lang>/<topic>.html, compiled in as :/info/<lang>/<topic>.html
file(GLOB_RECURSE GPIOCORE_HELP_FILES CONFIGURE_DEPENDS
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} i18n/info/*.html)
qt_add_resources(gpiocore_ui "help_texts" PREFIX "/info" BASE "i18n/info" FILES ${GPIOCORE_HELP_FILES})

add_executable(GPIOcoreGUI src/main.cpp)
target_link_libraries(GPIOcoreGUI PRIVATE gpiocore_ui)
target_compile_definitions(GPIOcoreGUI PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
set_target_properties(GPIOcoreGUI PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE)

# UI translations (Qt 6): i18n/gpiocore_<lang>.ts, edited with Qt Linguist.
#   cmake --build <build dir> --target update_translations   (refresh the .ts files)
# The .qm files are built and compiled in as :/i18n/gpiocore_<lang>.qm.
set(GPIOCORE_LANGUAGES nl CACHE STRING "UI languages besides English (i18n/gpiocore_<lang>.ts)")
if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 QUIET COMPONENTS LinguistTools)
    if(Qt6LinguistTools_FOUND)
        set(GPIOCORE_TS_FILES)
        foreach(lang IN LISTS GPIOCORE_LANGUAGES)
            list(APPEND GPIOCORE_TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/i18n/gpiocore_${lang}.ts)
        endforeach()
        qt_add_translations(gpiocore_ui
            TS_FILES ${GPIOCORE_TS_FILES}
            SOURCE_TARGETS gpiocore_core gpiocore_ui GPIOcoreGUI
            RESOURCE_PREFIX "/i18n"
            LUPDATE_OPTIONS -no-obsolete)
    endif()
endif()

# Unit tests (protocol, model, device link against the firmware simulator)
enable_testing()
add_executable(tst_gpiocore tests/tst_gpiocore.cpp)
target_link_libraries(tst_gpiocore PRIVATE gpiocore_core Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME tst_gpiocore COMMAND tst_gpiocore)

# Main window driven against the simulator
add_executable(tst_gui tests/tst_gui.cpp)
target_link_libraries(tst_gui PRIVATE gpiocore_ui Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME tst_gui COMMAND tst_gui -platform offscreen)
