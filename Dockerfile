FROM ubuntu:focal AS base

#RUN sed -i 's/\(deb .*\)/\1 non-free/' /etc/apt/sources.list && \
RUN apt update && apt install -y \
    libavdevice58 libavcodec58 \
    libavfilter7 libavformat58 libavresample4 libavutil56 libpostproc55 libswscale5 \
    ccache yasm \
    libdc1394-utils libv4l2rds0 liblapacke libatlas3-base libopenblas-base liblapack3 \
    libgstreamer1.0-0 libgstreamer-plugins-base1.0-0 \
    libgtk2.0-0 libatk1.0-0 libcanberra-gtk-module libopenexr24 \
    libssh-4 \
    tesseract-ocr

FROM base AS nodebase
RUN apt update && apt install -y curl \
    && curl -fsSL https://deb.nodesource.com/setup_25.x | bash - \
    && apt-get install -y nodejs

###
# Common lib for build
FROM nodebase AS buildbase

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
    libgtk2.0-dev git \
    libtesseract-dev libleptonica-dev

WORKDIR /usr/src

RUN git clone --branch 4.4.0 --depth=1 https://github.com/opencv/opencv.git
RUN git clone --branch 4.4.0 --depth=1 https://github.com/opencv/opencv_contrib.git

RUN mkdir opencv/build && \
    cd opencv/build && \
    cmake -D CMAKE_BUILD_TYPE=Release \
          -D CMAKE_INSTALL_PREFIX=/usr/opencv \
          -D OPENCV_GENERATE_PKGCONFIG=ON \
          -D BUILD_LIST=core,imgproc,dnn,objdetect,video,highgui,face,tracking,videoio,bgsegm \
          -D OPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules/ \
          -D WITH_OPENGL=ON \
          -D ENABLE_FAST_MATH=1 -D WITH_CUBLAS=1 \
          -D WITH_MKL=ON -DMKL_USE_MULTITHREAD=ON -DMKL_WITH_TBB=ON -DWITH_TBB=ON \
    -S .. -B .

WORKDIR /usr/src/opencv/build
RUN make -j4 install && make clean

###
# IA
FROM ubuntu:focal AS ia    

# Install Yolo v3
# https://pjreddie.com/darknet/yolo/
RUN apt update && apt install -y wget git && \
    mkdir -p /data/yolo && wget -O /data/yolo/yolov3-tiny.weights https://data.pjreddie.com/files/yolov3-tiny.weights && \
    git clone --depth 1 https://github.com/pjreddie/darknet.git /tmp/darknet && \
    # wget -O /data/yolo/yolov3-tiny.cfg https://raw.githubusercontent.com/pjreddie/darknet/refs/heads/master/cfg/yolov3-tiny.cfg && \
    cp /tmp/darknet/cfg/yolov3-tiny.cfg /tmp/darknet/data/coco.names /data/yolo/ && \
    rm -rf /tmp/darknet
    # wget -O /data/yolo/coco.names https://raw.githubusercontent.com/pjreddie/darknet/master/data/coco.names

# Detecção de faces
# RUN mkdir /data/faces && \
#     wget -O /data/faces/openface.nn4.small2.v1.t7 https://storage.cmusatyalab.org/openface-models/nn4.small2.v1.t7
# RUN git clone https://github.com/opencv/opencv_3rdparty.git /tmp/opencv_3rdparty && \
#     cp /tmp/opencv_3rdparty/dnn_samples_face_detector_20180205_fp16/res10_300x300_ssd_iter_140000_fp16.caffemodel /data/faces/ && \
#     rm -rf /tmp/opencv_3rdparty
# COPY --from=opencv /usr/src/opencv/samples/dnn/face_detector/deploy_lowres.prototxt /data/faces/

###
# Atalaia build-base
FROM buildbase AS atalaiabuildbase
RUN apt install -y libssh-dev

###
# DevContainer
FROM atalaiabuildbase AS devcontainer
RUN yes | unminimize && apt install -y git vim less gdb amqp-tools man-db
COPY --from=opencv /usr/opencv/ /usr/
COPY --from=ia /data /workspace/data
RUN mkdir -p /workspace/data/local

###
# Build
FROM atalaiabuildbase AS build
COPY --from=opencv /usr/opencv/ /usr/

WORKDIR /usr/src/atalaia
COPY CMakeLists.txt .
COPY atalaia-streaming atalaia-streaming
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
WORKDIR /usr/src/atalaia/build
#RUN apt install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
RUN make -j

###
# Final
FROM base AS final

RUN mkdir -p /data/local
COPY --from=opencv /usr/opencv/ /usr/
COPY --from=ia /data /data
COPY --from=build /usr/src/atalaia/build/atalaia-streaming/atalaia-streaming /usr/local/bin
WORKDIR /
ENTRYPOINT ["atalaia-streaming"]
# VOLUME /data/local

FROM nodebase AS node

RUN mkdir -p /data/local
COPY --from=opencv /usr/opencv/ /usr/
COPY --from=ia /data /data
COPY --from=build /usr/src/atalaia/build/atalaia-streaming/atalaia-streaming /usr/local/bin
WORKDIR /

ENTRYPOINT ["node"]