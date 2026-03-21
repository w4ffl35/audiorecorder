cmake_minimum_required(VERSION 3.21)

project(audiorecorder VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

include(FetchContent)

FetchContent_Declare(
    miniaudio
    URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.23.tar.gz
)
FetchContent_MakeAvailable(miniaudio)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Threads REQUIRED)

qt_standard_project_setup()

qt_add_executable(audiorecorder
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/AudioRecorderWorker.h
    src/AudioRecorderWorker.cpp
    src/LevelMeterWidget.h
    src/LevelMeterWidget.cpp
    src/WavWriter.h
    src/WavWriter.cpp
    src/miniaudio_impl.cpp
)

target_include_directories(audiorecorder PRIVATE ${miniaudio_SOURCE_DIR})

target_link_libraries(audiorecorder
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Threads::Threads
)

if(WIN32)
    target_link_libraries(audiorecorder PRIVATE ole32 avrt)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(audiorecorder PRIVATE dl m)
endif()
