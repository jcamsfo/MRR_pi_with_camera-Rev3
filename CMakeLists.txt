cmake_minimum_required(VERSION 3.10)

project(MRR_Pi)


find_package(PkgConfig REQUIRED)
pkg_check_modules(AVFORMAT libavformat REQUIRED)
pkg_check_modules(AVCODEC libavcodec REQUIRED)
pkg_check_modules(AVUTIL libavutil REQUIRED)
pkg_check_modules(SWSCALE libswscale REQUIRED)
find_package(OpenCV REQUIRED)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include_directories(../)

include_directories(${AVFORMAT_INCLUDE_DIRS}
                    ${AVCODEC_INCLUDE_DIRS}
                    ${AVUTIL_INCLUDE_DIRS}
                    ${SWSCALE_INCLUDE_DIRS}
                    ${OpenCV_INCLUDE_DIRS})



add_executable(${PROJECT_NAME}_server server.cpp comms.cpp mixer_processor.cpp comms.h)

target_link_libraries(${PROJECT_NAME}_server ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries(${PROJECT_NAME}_server ${AVFORMAT_LIBRARIES}
                      ${AVCODEC_LIBRARIES}
                      ${AVUTIL_LIBRARIES}
                      ${SWSCALE_LIBRARIES}
                      ${OpenCV_LIBS})


add_executable(${PROJECT_NAME}_client client.cpp comms.cpp comms.h camera_grab.cpp file_io.cpp client_params.cpp)

target_link_libraries(${PROJECT_NAME}_client ${CMAKE_THREAD_LIBS_INIT})                      

target_link_libraries(${PROJECT_NAME}_client ${AVFORMAT_LIBRARIES}
${AVCODEC_LIBRARIES}
${AVUTIL_LIBRARIES}
${SWSCALE_LIBRARIES}
${OpenCV_LIBS})




# to build xcode project
#   cd xbuild
#   cmake .. -GXcode



