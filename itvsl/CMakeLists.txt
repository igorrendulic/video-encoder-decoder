cmake_minimum_required(VERSION 3.17)
project(itvsl C CXX)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(proto)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavdevice
    libavfilter
    libavformat
    libavcodec
    libswresample
    libswscale
    libavutil
)

list(APPEND SOURCES
    video_log.cpp
    video_log.h
    itvsl_archive.h
    itvsl_archive.cpp
    mp4_file_cleanup.h
    mp4_file_cleanup.cpp
    itvsl.cpp
    itvsl.h
    itvsl_queue.h
)

# add_executable(libvsldecoder ${SOURCES})
add_library(itvsl ${SOURCES})

target_link_libraries(itvsl
    # ${OpenCV_LIBS}
    proto ${PROTOBUF_LIBRARY}
    PkgConfig::LIBAV
)

install(TARGETS itvsl DESTINATION lib)
install(FILES itvsl.h DESTINATION include)
