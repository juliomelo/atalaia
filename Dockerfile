FROM debian:buster as base

RUN apt update && apt install -y \
    libavdevice58 libavcodec58 \
    #libopencv-contrib3.2 libopencv-core3.2 libopencv-objdetect3.2 libopencv-imgproc3.2

# Install Yolo v3
RUN apt install -y wget && \
    mkdir -p /data/yolo && wget -O /data/yolo/yolov3-tiny.weights https://pjreddie.com/media/files/yolov3-tiny.weights && \
    wget -O /data/yolo/yolov3-tiny.cfg https://raw.githubusercontent.com/pjreddie/darknet/master/cfg/yolov3-tiny.cfg && \
    wget -O /data/yolo/coco.names https://raw.githubusercontent.com/pjreddie/darknet/master/data/coco.names && \
    mkdir /data/local

FROM base AS opencv

RUN apt install -y \
    libavcodec58 libavfilter7 libavformat58 libavresample4 libavutil56 libpostproc55 libswscale5 \
    ccache build-essential pkg-config yasm \
    libdc1394-utils libv4l2rds0 liblapacke libatlas3-base libopenblas-base liblapack3 liblapacke \
    libavfilter-dev libavutil-dev \
    libpostproc-dev libswscale-dev libavcodec-dev libavformat-dev libswscale-dev libgstreamer-plugins-base1.0-dev \
    libgstreamer1.0-dev libpng-dev libjpeg-dev libopenexr-dev libtiff-dev libwebp-dev  \
    libdc1394-22-dev liblapack-dev libeigen3-dev libopenblas-dev libatlas-base-dev liblapacke-dev \
    procps iproute2 net-tools cmake ffmpeg libavcodec58 libavfilter7 libavformat58 libavresample4 libavutil56 libavcodec-dev libavfilter-dev libavutil-dev libpostproc-dev libpostproc55 libswscale-dev libswscale5 libavcodec-dev libavformat-dev libswscale-dev libgstreamer-plugins-base1.0-dev libgstreamer1.0-dev libpng-dev libjpeg-dev libopenexr-dev libtiff-dev libwebp-dev ccache libdc1394-22-dev libdc1394-utils libv4l2rds0 liblapack-dev libeigen3-dev libopenblas-dev libatlas-base-dev liblapacke liblapacke-dev \
    libgtk2.0-dev

WORKDIR /usr/src

RUN git clone --branch 4.2.0 --depth=1 https://github.com/opencv/opencv.git
RUN git clone --branch 4.2.0 --depth=1 https://github.com/opencv/opencv_contrib.git

RUN mkdir opencv/build && \
    cd opencv/build && \
    cmake -D CMAKE_BUILD_TYPE=Release \
#          -D CMAKE_INSTALL_PREFIX=/usr \
          -D OPENCV_GENERATE_PKGCONFIG=ON \
#          -D BUILD_LIST=core,imgproc,dnn,objdetect,video,highgui,face \
          -D OPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules \
#          -D WITH_OPENGL=ON \
          -S .. -B . && \
    make -j4 install || make install

# Build
FROM base as build

RUN apt install -y ffmpeg git cmake \
    procps iproute2 net-tools \
    build-essential pkg-config \
    libavdevice-dev libavcodec-dev \
    #libopencv-dev libopencv-core-dev libopencv-objdetect-dev libopencv-imgproc-dev \
    #libopencv-contrib-dev \
    #libopencv-contrib3.2 libopencv-core3.2 libopencv-objdetect3.2 libopencv-imgproc3.2 \
    #opencv-doc

WORKDIR /usr/src/atalaia
COPY . .
RUN cmake -S . -B build
WORKDIR /usr/src/atalaia/build
RUN make -j

# Final
FROM base as final
COPY --from=build /usr/src/atalaia/build/atalaia-streaming/atalaia-streaming /usr/local/bin
WORKDIR /data
ENTRYPOINT ["atalaia-streaming"]
VOLUME /data/local