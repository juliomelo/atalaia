FROM debian:buster as base

RUN apt update && apt install -y \
    libavdevice58 libavcodec58 \
    libavfilter7 libavformat58 libavresample4 libavutil56 libpostproc55 libswscale5 \
    ccache yasm \
    libdc1394-utils libv4l2rds0 liblapacke libatlas3-base libopenblas-base liblapack3 \
    libgstreamer1.0-0 libgstreamer-plugins-base1.0 \
    libgtk2.0-0 libatk1.0-0 libcanberra-gtk-module
#    libssh-4

###
# IA
FROM debian:buster as ia    

# Install Yolo v3
RUN apt update && apt install -y wget && \
    mkdir -p /data/yolo && wget -O /data/yolo/yolov3-tiny.weights https://pjreddie.com/media/files/yolov3-tiny.weights && \
    wget -O /data/yolo/yolov3-tiny.cfg https://raw.githubusercontent.com/pjreddie/darknet/master/cfg/yolov3-tiny.cfg && \
    wget -O /data/yolo/coco.names https://raw.githubusercontent.com/pjreddie/darknet/master/data/coco.names

###
# Common lib for build
FROM base as buildbase

RUN apt install -y ffmpeg cmake \
    #procps iproute2 net-tools \
    build-essential pkg-config \
    libavdevice-dev libavcodec-dev


###
# Build OpenCV
FROM buildbase AS opencv

RUN apt install -y \
    libavfilter-dev libavutil-dev \
    libpostproc-dev libswscale-dev libavcodec-dev libavformat-dev libgstreamer-plugins-base1.0-dev \
    libgstreamer1.0-dev libpng-dev libjpeg-dev libopenexr-dev libtiff-dev libwebp-dev  \
    libdc1394-22-dev liblapack-dev libeigen3-dev libopenblas-dev libatlas-base-dev liblapacke-dev \
    libgtk2.0-dev git

WORKDIR /usr/src

RUN git clone --branch 4.2.0 --depth=1 https://github.com/opencv/opencv.git
RUN git clone --branch 4.2.0 --depth=1 https://github.com/opencv/opencv_contrib.git

RUN mkdir opencv/build && \
    cd opencv/build && \
    cmake -D CMAKE_BUILD_TYPE=Release \
          -D CMAKE_INSTALL_PREFIX=/usr/opencv \
          -D OPENCV_GENERATE_PKGCONFIG=ON \
          -D BUILD_LIST=core,imgproc,dnn,objdetect,video,highgui,face,tracking,videoio \
          -D OPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules \
          -D WITH_OPENGL=ON \
          -S .. -B .

WORKDIR /usr/src/opencv/build
RUN make -j4 install || make install

###
# Atalaia build-base
FROM buildbase as atalaiaBuildBase
RUN apt install -y libssh-dev

###
# DevContainer
FROM atalaiaBuildBase as devcontainer
RUN apt install -y git vim less gdb amqp-tools
COPY --from=opencv /usr/opencv/ /usr/
COPY --from=ia /data /data

###
# Build
FROM atalaiaBuildBase as build
COPY --from=opencv /usr/opencv/ /usr/

WORKDIR /usr/src/atalaia
COPY . .
RUN cmake -S . -B build
WORKDIR /usr/src/atalaia/build
#RUN apt install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
RUN make -j

###
# Final
FROM base as final

RUN mkdir -p /data/local
COPY --from=opencv /usr/opencv/ /usr/
COPY --from=ia /data /data
COPY --from=build /usr/src/atalaia/build/atalaia-streaming/atalaia-streaming /usr/local/bin
WORKDIR /data
ENTRYPOINT ["atalaia-streaming"]
VOLUME /data/local