cmake_minimum_required(VERSION 3.21)

project(audiorecorder VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    miniaudio
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.23.tar.gz
)
FetchContent_MakeAvailable(miniaudio)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Threads REQUIRED)

qt_standard_project_setup()

set(AUDIORECORDER_PLATFORM_SOURCE src/AudioRecorderWorkerPlatform_unsupported.cpp)

if(WIN32)
    set(AUDIORECORDER_PLATFORM_SOURCE src/AudioRecorderWorkerPlatform_windows.cpp)
elseif(UNIX AND NOT APPLE)
    set(AUDIORECORDER_PLATFORM_SOURCE src/AudioRecorderWorkerPlatform_linux.cpp)
endif()

qt_add_executable(audiorecorder
    src/main.cpp
    src/MainWindow.h
    src/MainWindowUi.h
    src/MainWindow.cpp
    src/MainWindowUi.cpp
    src/AudioRecorderWorker.h
    src/AudioRecorderWorkerConfig.h
    src/AudioRecorderWorkerState.h
    src/AudioRecorderWorker.cpp
    src/AudioRecorderWorkerPlatform.h
    src/LevelMeterWidget.h
    src/LevelMeterWidget.cpp
    src/WavWriter.h
    src/WavWriter.cpp
    src/WavWriterPcm.h
    src/WavWriterPcm.cpp
    src/WavWriterStream.h
    src/WavWriterStream.cpp
    src/miniaudio_impl.cpp
    ${AUDIORECORDER_PLATFORM_SOURCE}
)

set_target_properties(audiorecorder PROPERTIES AUTOMOC ON)

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
