# CMakeList.txt: projeto de CMake para Atalaia-Streaming, incluir origem e definir
# lógica específica a um projeto aqui.
#
cmake_minimum_required (VERSION 3.8)

# Adicione a origem ao executável deste projeto.
#add_executable (atalaia-Streaming "Atalaia-Streaming.cpp" "Atalaia-Streaming.h")

set(CMAKE_WARN_DEPRECATED FALSE)
add_compile_options(-Wno-deprecated-declarations)

# TODO: Adicione testes e instale destinos, se necessário.
add_executable(atalaia-streaming
    main.cpp
    CameraAtalaia.cpp CameraAtalaia.hpp
    MovementDetector.cpp MovementDetector.hpp
    ObjectDetector.cpp ObjectDetector.hpp
    VideoStream.cpp VideoStream.hpp
    util/BlockingQueue.hpp)

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY avcodec)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)

find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
find_library(AVUTIL_LIBRARY avutil)

find_path(AVDEVICE_INCLUDE_DIR libavdevice/avdevice.h)
find_library(AVDEVICE_LIBRARY avdevice)

find_package(OpenCV 4 REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
link_directories(${OpenCV_LIBRARY_DIRS})

find_package (Threads)

find_library(SWSCALE_LIBRARY swscale)

target_include_directories(atalaia-streaming PRIVATE ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVDEVICE_INCLUDE_DIR} ${OPENCV_INCLUDE_DIR})
target_link_libraries(atalaia-streaming PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY} ${OpenCV_LIBS} ${SWSCALE_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})

