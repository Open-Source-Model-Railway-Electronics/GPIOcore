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
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort Network Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort Network Test)

# ── The firmware on the PC ──────────────────────────────────────────────────
# gpiocore_fw_host runs the real firmware (software/GPIOcore) with an Arduino shim (fwhost/).
# The simulator (src/simulateddevice.cpp) starts it next to the GUI / test exe and talks to it
# through stdin / stdout. No Qt, plain C++. NmraDcc.cpp is left out: there is no DCC signal, the
# host program calls the firmware's DCC callbacks itself.
set(GPIOCORE_FIRMWARE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../GPIOcore)
file(GLOB GPIOCORE_FW_SOURCES CONFIGURE_DEPENDS
     ${GPIOCORE_FIRMWARE_DIR}/*.cpp ${GPIOCORE_FIRMWARE_DIR}/src/*.cpp)
list(FILTER GPIOCORE_FW_SOURCES EXCLUDE REGEX "/NmraDcc\\.cpp$")
add_executable(gpiocore_fw_host
    fwhost/main.cpp fwhost/hostapi.h
    fwhost/sketch.cpp fwhost/arduino_shim.cpp
    ${GPIOCORE_FW_SOURCES})
set_target_properties(gpiocore_fw_host PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(gpiocore_fw_host PRIVATE fwhost fwhost/arduino ${GPIOCORE_FIRMWARE_DIR})
# ARDUINO: NmraDcc.h includes Arduino.h. weak=unused: the firmware declares its callbacks
# __attribute__((weak)); MinGW's linker resolves a weak reference to a weak definition in another
# object file to null, so they would never be called. The firmware defines all of them anyway.
target_compile_definitions(gpiocore_fw_host PRIVATE ARDUINO=10819 weak=unused)
# The firmware's EEPROM structs need the AVR layout: packed, bit-fields across byte boundaries
# (static_asserts in GPIOcore.ino / input.h check it). Not for main.cpp: it uses the C++ library.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(GPIOCORE_FW_FLAGS -fpack-struct -Wno-packed-bitfield-compat)
    if(MINGW)
        list(APPEND GPIOCORE_FW_FLAGS -mno-ms-bitfields)
    endif()
    set_source_files_properties(fwhost/sketch.cpp fwhost/arduino_shim.cpp ${GPIOCORE_FW_SOURCES}
                                PROPERTIES COMPILE_OPTIONS "${GPIOCORE_FW_FLAGS}")
else()
    message(WARNING "gpiocore_fw_host needs GCC or Clang (AVR struct layout); the simulator will not start")
endif()
if(WIN32)
    target_link_libraries(gpiocore_fw_host PRIVATE winmm)
endif()
if(MINGW)
    target_link_options(gpiocore_fw_host PRIVATE -static)   # no MinGW runtime DLLs needed
endif()

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
# The simulator starts gpiocore_fw_host from next to the exe; this path is the fallback (build tree).
add_dependencies(gpiocore_core gpiocore_fw_host)
target_compile_definitions(gpiocore_core PRIVATE GPIOCORE_FW_HOST_PATH="$<TARGET_FILE:gpiocore_fw_host>")

add_library(gpiocore_ui STATIC
    src/mainwindow.h      src/mainwindow.cpp
    src/outputeditor.h    src/outputeditor.cpp
    src/inputeditor.h     src/inputeditor.cpp
    src/pinmapview.h      src/pinmapview.cpp
    src/connectionview.h  src/connectionview.cpp
    src/uihelpers.h       src/uihelpers.cpp
    src/flasher.h         src/flasher.cpp
    src/qrcode.h          src/qrcode.cpp
    src/webremote.h       src/webremote.cpp
    src/infotexts.h       src/infotexts.cpp
    src/i18n.h            src/i18n.cpp
)
target_link_libraries(gpiocore_ui PUBLIC gpiocore_core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

# Help texts: i18n/info/<lang>/<topic>.html, compiled in as :/info/<lang>/<topic>.html
file(GLOB_RECURSE GPIOCORE_HELP_FILES CONFIGURE_DEPENDS
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} i18n/info/*.html)
qt_add_resources(gpiocore_ui "help_texts" PREFIX "/info" BASE "i18n/info" FILES ${GPIOCORE_HELP_FILES})

add_executable(GPIOcoreGUI src/main.cpp)
target_link_libraries(GPIOcoreGUI PRIVATE gpiocore_ui)

# small UI images (check mark for check boxes)
qt_add_resources(gpiocore_ui "ui_images" PREFIX "/ui" BASE "ui" FILES ui/check.svg)

# Firmware flashing: avrdude next to the exe, dev builds also find tools/ and release/ in the source tree
target_compile_definitions(gpiocore_ui PRIVATE GPIOCORE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_custom_command(TARGET GPIOcoreGUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/avrdude
            $<TARGET_FILE_DIR:GPIOcoreGUI>/tools/avrdude)
target_compile_definitions(GPIOcoreGUI PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
set_target_properties(GPIOcoreGUI PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE)

# UI translations (Qt 6): i18n/gpiocore_<lang>.ts, edited with Qt Linguist.
#   cmake --build <build dir> --target update_translations   (refresh the .ts files)
# The .qm files are built and compiled in as :/i18n/gpiocore_<lang>.qm.
set(GPIOCORE_LANGUAGES nl de fr es it pl cs da nb sv hu pt CACHE STRING "UI languages besides English (i18n/gpiocore_<lang>.ts)")
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
