cmake_minimum_required(VERSION 3.16)

project(IOcoreGUI VERSION 0.1.0 LANGUAGES CXX)

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
# The GUI plays the DFPlayer's tracks (src/soundplayer.cpp, Qt 6 API): Windows Media Foundation plugin, no FFmpeg
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Multimedia)

# ── The firmware on the PC ──────────────────────────────────────────────────
# iocore_fw_host runs the real firmware (software/IOcore) with an Arduino shim (fwhost/).
# The simulator (src/simulateddevice.cpp) starts it next to the GUI / test exe and talks to it
# through stdin / stdout. No Qt, plain C++. NmraDcc.cpp is left out: there is no DCC signal, the
# host program calls the firmware's DCC callbacks itself.
set(IOCORE_FIRMWARE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../IOcore)
file(GLOB IOCORE_FW_SOURCES CONFIGURE_DEPENDS
     ${IOCORE_FIRMWARE_DIR}/*.cpp ${IOCORE_FIRMWARE_DIR}/src/*.cpp)
list(FILTER IOCORE_FW_SOURCES EXCLUDE REGEX "/NmraDcc\\.cpp$")
add_executable(iocore_fw_host
    fwhost/main.cpp fwhost/hostapi.h
    fwhost/sketch.cpp fwhost/arduino_shim.cpp
    ${IOCORE_FW_SOURCES})
set_target_properties(iocore_fw_host PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_include_directories(iocore_fw_host PRIVATE fwhost fwhost/arduino ${IOCORE_FIRMWARE_DIR})
# ARDUINO: NmraDcc.h includes Arduino.h. weak=unused: the firmware declares its callbacks
# __attribute__((weak)); MinGW's linker resolves a weak reference to a weak definition in another
# object file to null, so they would never be called. The firmware defines all of them anyway.
target_compile_definitions(iocore_fw_host PRIVATE ARDUINO=10819 weak=unused IOCORE_HOST)
# The firmware's EEPROM structs need the AVR layout: packed, bit-fields across byte boundaries
# (static_asserts in IOcore.ino / input.h check it). Not for main.cpp: it uses the C++ library.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(IOCORE_FW_FLAGS -fpack-struct -Wno-packed-bitfield-compat)
    if(MINGW)
        list(APPEND IOCORE_FW_FLAGS -mno-ms-bitfields)
    endif()
    set_source_files_properties(fwhost/sketch.cpp fwhost/arduino_shim.cpp ${IOCORE_FW_SOURCES}
                                PROPERTIES COMPILE_OPTIONS "${IOCORE_FW_FLAGS}")
else()
    message(WARNING "iocore_fw_host needs GCC or Clang (AVR struct layout); the simulator will not start")
endif()
if(WIN32)
    target_link_libraries(iocore_fw_host PRIVATE winmm)
endif()
if(MINGW)
    target_link_options(iocore_fw_host PRIVATE -static)   # no MinGW runtime DLLs needed
endif()

# Core logic without UI: protocol, config model, device link, simulator.
# Shared by the application and the unit tests.
add_library(iocore_core STATIC
    src/protocol.h        src/protocol.cpp
    src/gpioconfig.h      src/gpioconfig.cpp
    src/connections.h     src/connections.cpp
    src/boardtools.h      src/boardtools.cpp
    src/devicelink.h      src/devicelink.cpp
    src/simulateddevice.h src/simulateddevice.cpp
)
target_include_directories(iocore_core PUBLIC src)
target_link_libraries(iocore_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::SerialPort)
# The simulator starts iocore_fw_host from next to the exe; this path is the fallback (build tree).
add_dependencies(iocore_core iocore_fw_host)
target_compile_definitions(iocore_core PRIVATE IOCORE_FW_HOST_PATH="$<TARGET_FILE:iocore_fw_host>")

add_library(iocore_ui STATIC
    src/mainwindow.h      src/mainwindow.cpp
    src/outputeditor.h    src/outputeditor.cpp
    src/inputeditor.h     src/inputeditor.cpp
    src/connectionview.h  src/connectionview.cpp
    src/uihelpers.h       src/uihelpers.cpp
    src/flasher.h         src/flasher.cpp
    src/qrcode.h          src/qrcode.cpp
    src/webremote.h       src/webremote.cpp
    src/infotexts.h       src/infotexts.cpp
    src/i18n.h            src/i18n.cpp
    src/soundplayer.h     src/soundplayer.cpp
)
target_link_libraries(iocore_ui PUBLIC iocore_core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network
                      Qt${QT_VERSION_MAJOR}::Multimedia)

# Help texts: i18n/info/<lang>/<topic>.html, compiled in as :/info/<lang>/<topic>.html
file(GLOB_RECURSE IOCORE_HELP_FILES CONFIGURE_DEPENDS
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} i18n/info/*.html)
qt_add_resources(iocore_ui "help_texts" PREFIX "/info" BASE "i18n/info" FILES ${IOCORE_HELP_FILES})

add_executable(IOcoreGUI src/main.cpp)
target_link_libraries(IOcoreGUI PRIVATE iocore_ui)
if(WIN32)
    target_sources(IOcoreGUI PRIVATE ui/iocore.rc)   # the OSHW icon on the exe (ui/make_icon.py)
endif()

# small UI images (check mark for check boxes, the OSHW window icon)
qt_add_resources(iocore_ui "ui_images" PREFIX "/ui" BASE "ui" FILES ui/check.svg ui/oshw.svg)

# Firmware flashing: avrdude next to the exe, dev builds also find tools/ and release/ in the source tree
target_compile_definitions(iocore_ui PRIVATE IOCORE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_custom_command(TARGET IOcoreGUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/avrdude
            $<TARGET_FILE_DIR:IOcoreGUI>/tools/avrdude)
target_compile_definitions(IOcoreGUI PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
set_target_properties(IOcoreGUI PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE)

# UI translations (Qt 6): i18n/iocore_<lang>.ts, edited with Qt Linguist.
#   cmake --build <build dir> --target update_translations   (refresh the .ts files)
# The .qm files are built and compiled in as :/i18n/iocore_<lang>.qm.
set(IOCORE_LANGUAGES nl de fr es it pl cs da nb sv hu pt CACHE STRING "UI languages besides English (i18n/iocore_<lang>.ts)")
if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 QUIET COMPONENTS LinguistTools)
    if(Qt6LinguistTools_FOUND)
        set(IOCORE_TS_FILES)
        foreach(lang IN LISTS IOCORE_LANGUAGES)
            list(APPEND IOCORE_TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/i18n/iocore_${lang}.ts)
        endforeach()
        qt_add_translations(iocore_ui
            TS_FILES ${IOCORE_TS_FILES}
            SOURCE_TARGETS iocore_core iocore_ui IOcoreGUI
            RESOURCE_PREFIX "/i18n"
            LUPDATE_OPTIONS -no-obsolete)
    endif()
endif()

# Unit tests (protocol, model, device link against the firmware simulator)
enable_testing()
add_executable(tst_iocore tests/tst_iocore.cpp)
target_link_libraries(tst_iocore PRIVATE iocore_core Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME tst_iocore COMMAND tst_iocore)

# Main window driven against the simulator
add_executable(tst_gui tests/tst_gui.cpp)
target_link_libraries(tst_gui PRIVATE iocore_ui Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME tst_gui COMMAND tst_gui -platform offscreen)
