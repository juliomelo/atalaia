#See https://aka.ms/containerfastmode to understand how Visual Studio uses this Dockerfile to build your images for faster debugging.

#FROM debian:buster-slim AS opencv
#RUN apt update && \
    #apt install -y git \
    #cmake libavcodec-dev libavfilter-dev libavutil-dev \
    #libpostproc-dev libswscale-dev libavcodec-dev libavformat-dev libswscale-dev libgstreamer-plugins-base1.0-dev \
    #libgstreamer1.0-dev libpng-dev libjpeg-dev libopenexr-dev libtiff-dev libwebp-dev libcaffe-cpu-dev  \
    #libdc1394-22-dev liblapack-dev libeigen3-dev libopenblas-dev libatlas-base-dev liblapacke-dev
#WORKDIR /usr/src
#RUN git clone --branch 4.2.0 https://github.com/opencv/opencv.git
#RUN git clone --branch 4.2.0 https://github.com/opencv/opencv_contrib.git
#RUN mkdir opencv/build && \
    #cd opencv/build && \
    #cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=/usr/local -D OPENCV_GENERATE_PKGCONFIG=ON -D BUILD_LIST=core,imgproc,dnn,objdetect,video -S .. -B . && \
    #make -j4 || make
#COPY --from=opencv /usr/src/opencv/build/lib/*.so /usr/lib/
#COPY --from=opencv /usr/src/opencv/build/lib/*.so.* /usr/lib/
##
# objdetect e video dependem de : calib3d,features2d,flann

FROM mcr.microsoft.com/dotnet/core/runtime:3.1-buster-slim AS base
RUN apt update && \
    apt install -y ffmpeg \
    libavcodec58 libavfilter7 libavformat58 libavresample4 libavutil56 libpostproc55 libswscale5 \
    caffe-tools-cpu caffe-cpu ccache \
    libdc1394-utils libv4l2rds0 liblapacke libatlas3-base libopenblas-base liblapack3 liblapacke \
    \
    git \
    cmake libavcodec-dev libavfilter-dev libavutil-dev \
    libpostproc-dev libswscale-dev libavcodec-dev libavformat-dev libswscale-dev libgstreamer-plugins-base1.0-dev \
    libgstreamer1.0-dev libpng-dev libjpeg-dev libopenexr-dev libtiff-dev libwebp-dev libcaffe-cpu-dev  \
    libdc1394-22-dev liblapack-dev libeigen3-dev libopenblas-dev libatlas-base-dev liblapacke-dev \
    procps iproute2 net-tools cmake ffmpeg libavcodec58 libavfilter7 libavformat58 libavresample4 libavutil56 libavcodec-dev libavfilter-dev libavutil-dev libpostproc-dev libpostproc55 libswscale-dev libswscale5 libavcodec-dev libavformat-dev libswscale-dev libgstreamer-plugins-base1.0-dev libgstreamer1.0-dev libpng-dev libjpeg-dev libopenexr-dev libtiff-dev libwebp-dev libcaffe-cpu-dev caffe-tools-cpu caffe-cpu ccache libdc1394-22-dev libdc1394-utils libv4l2rds0 liblapack-dev libeigen3-dev libopenblas-dev libatlas-base-dev liblapacke liblapacke-dev
WORKDIR /usr/src
RUN git clone --branch 4.2.0 https://github.com/opencv/opencv.git
RUN git clone --branch 4.2.0 https://github.com/opencv/opencv_contrib.git
RUN apt install -y build-essential pkg-config yasm

RUN mkdir opencv/build && \
    cd opencv/build && \
    cmake -D CMAKE_BUILD_TYPE=Release \
          -D CMAKE_INSTALL_PREFIX=/usr \
          -D OPENCV_GENERATE_PKGCONFIG=ON \
#          -D BUILD_LIST=core,imgproc,dnn,objdetect,video \
          -D OPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules \
#          -D WITH_OPENGL=ON \
          -S .. -B . && \
    make -j4 install || make install

RUN git clone --branch 4.2.0.20200108 https://github.com/shimat/opencvsharp.git
RUN cd opencvsharp/src && \
    cmake -D CMAKE_INSTALL_PREFIX=/usr . && \
    export CPLUS_INCLUDE_PATH=$(find /usr/src -name "include" | paste -sd ':' -) && \
    make && \
    make install
RUN cd /usr/lib && ln -s libOpenCvSharpExtern.so libOpenCvSharpExtern

ADD https://pjreddie.com/media/files/yolov3-tiny.weights /data/yolo/yolov3-tiny.weights
ADD https://raw.githubusercontent.com/pjreddie/darknet/master/cfg/yolov3-tiny.cfg /data/yolo/yolov3-tiny.cfg
ADD https://raw.githubusercontent.com/pjreddie/darknet/master/data/coco.names /data/yolo/coco.names
RUN mkdir /data/local
WORKDIR /app

FROM mcr.microsoft.com/dotnet/core/sdk:3.1-buster AS build
WORKDIR /src
COPY ["atalaia-streaming/atalaia-streaming.csproj", "atalaia-streaming/"]
RUN dotnet restore "atalaia-streaming/atalaia-streaming.csproj"
COPY . .
WORKDIR "/src/atalaia-streaming"
RUN dotnet build "atalaia-streaming.csproj" -c Release -o /app/build

FROM build AS publish
RUN dotnet publish "atalaia-streaming.csproj" -c Release -o /app/publish

FROM base AS final
WORKDIR /app
COPY --from=publish /app/publish .
ENTRYPOINT ["dotnet", "atalaia-streaming.dll"]
VOLUME /data/local