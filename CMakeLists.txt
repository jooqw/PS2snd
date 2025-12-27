cmake_minimum_required(VERSION 3.16)
project(ps2snd LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Grab sf2cute sources
file(GLOB SF2CUTE_SOURCES "libs/sf2cute/src/sf2cute/*.cpp")

if(NOT SF2CUTE_SOURCES)
    message(FATAL_ERROR "sf2cute sources not found! Check libs/sf2cute/src/sf2cute/")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/ui/ps2snd.cpp
    src/hd.cpp
    src/bd.cpp
    src/2sf2.cpp
    src/ui/waveform.cpp
    ${SF2CUTE_SOURCES}
)

set(HEADERS
    src/main.h
    src/ui/ps2snd.h
    src/hd.h
    src/bd.h
    src/2sf2.h
    src/ui/waveform.h
)

set(UI_FILES
    src/ui/ps2snd.ui
)

add_executable(ps2snd ${SOURCES} ${HEADERS} ${UI_FILES})

# Fix sf2cute headers
target_precompile_headers(ps2snd PRIVATE <cstdint> <vector> <string> <memory> <algorithm> <cmath>)

target_include_directories(ps2snd PRIVATE
    src
    src/ui
    libs
    libs/sf2cute/include
)

target_link_libraries(ps2snd PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
