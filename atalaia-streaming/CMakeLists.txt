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
    util/BlockingQueue.hpp
    recorder/VideoStream.cpp recorder/VideoStream.hpp
    recorder/MovementDetector.cpp recorder/MovementDetector.hpp
    recorder/MotionRecorder.cpp recorder/MotionRecorder.hpp
    recorder/ObjectDetector.cpp recorder/ObjectDetector.hpp
    recorder/ObjectRecorder.cpp recorder/ObjectRecorder.hpp
    notify/Listener.hpp notify/Listener.cpp
    notify/Notify.hpp notify/LocalNotifier.cpp
    notify/amqp/AtalaiaTcpHandler.hpp notify/amqp/AtalaiaTcpHandler.cpp
    notify/amqp/AMQPNotifier.hpp notify/amqp/AMQPNotifier.cpp
)


find_library(AVCODEC_LIBRARY avcodec)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVUTIL_LIBRARY avutil)
find_library(AVDEVICE_LIBRARY avdevice)

find_package(OpenCV 4 REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
link_directories(${OpenCV_LIBRARY_DIRS})

#find_package(amqpcpp REQUIRED)
#set(LIBS ${LIBS} amqpcpp_LIBRARIES)
#include_directories(BEFORE ${amqpcpp_INCLUDE_DIR})
#set(INCLUDES ${INCLUDES} ${amqpcpp_INCLUDE_DIR})
#message(STATUS "Found RabbitMQ library: ${amqpcpp_LIBRARIES}")

include(ExternalProject)
set(AMQPCPP_ROOT "${CMAKE_CURRENT_BINARY_DIR}/amqpcpp")
ExternalProject_Add(libamqpcpp
    URL "https://github.com/CopernicaMarketingSoftware/AMQP-CPP/archive/v4.1.7.tar.gz"
    URL_MD5 "65af241c10a92904fefa3ddd10df0693"
    CMAKE_ARGS -Wno-dev "-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>" -DAMQP-CPP_LINUX_TCP=ON
    INSTALL_DIR ${AMQPCPP_ROOT}
)
set(AMQPCPP_INCLUDE_DIR ${AMQPCPP_ROOT}/include)
set(AMQPCPP_LIBRARIES ${AMQPCPP_ROOT}/lib/libamqpcpp.a)
message(STATUS "Found external dependency AMQP-CPP: " ${AMQPCPP_INCLUDE_DIR})
include_directories(${AMQPCPP_INCLUDE_DIR})
list(APPEND LIBS ${AMQPCPP_LIBRARIES})
list(APPEND DEPENDENCIES libamqpcpp)

find_package (Threads)

find_library(SWSCALE_LIBRARY swscale)

set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -ldl")

add_dependencies(atalaia-streaming ${DEPENDENCIES})

target_include_directories(atalaia-streaming PRIVATE ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVDEVICE_INCLUDE_DIR} ${OPENCV_INCLUDE_DIR} ${AMQPCPP_INCLUDE_DIR})
target_link_libraries(atalaia-streaming PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY} ${OpenCV_LIBS} ${SWSCALE_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${LIBS})

