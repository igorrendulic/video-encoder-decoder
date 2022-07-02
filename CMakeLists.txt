cmake_minimum_required (VERSION 3.14) 

set(CMAKE_BUILD_TYPE Debug)

project(vsl VERSION 1.0)

find_package (Threads)
# find_package(OpenCV REQUIRED )

# adding ItvsLib
add_subdirectory(itvsl) # igor technology video streaming library
list(APPEND EXTRA_LIB_DIRS "itvsl")
list(APPEND EXTRA_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/itvsl")
list(APPEND EXTRA_LINKS "itvsl")

add_executable (${PROJECT_NAME} main.cpp queue.h packet_reader.h)

target_include_directories(${PROJECT_NAME} PUBLIC ${EXTRA_INCLUDE_DIRS})

target_link_directories(${PROJECT_NAME} PRIVATE ${EXTRA_LIB_DIRS})

# target_link_libraries (cms ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_LINKS} ${OpenCV_LIBS})
target_link_libraries (vsl ${CMAKE_THREAD_LIBS_INIT} ${EXTRA_LINKS})