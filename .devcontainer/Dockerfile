FROM debian:buster

RUN apt update && apt install -y wget
RUN mkdir -p /data/yolo && wget -O /data/yolo/yolov3-tiny.weights https://pjreddie.com/media/files/yolov3-tiny.weights 
RUN wget -O /data/yolo/yolov3-tiny.cfg https://raw.githubusercontent.com/pjreddie/darknet/master/cfg/yolov3-tiny.cfg
RUN wget -O /data/yolo/coco.names https://raw.githubusercontent.com/pjreddie/darknet/master/data/coco.names
RUN mkdir /data/local

RUN apt install -y ffmpeg git cmake \
    procps iproute2 net-tools \
    build-essential pkg-config \
    libavdevice-dev libavcodec-dev \
    libopencv-dev libopencv-core-dev libopencv-objdetect-dev libopencv-imgproc-dev \
    libopencv-contrib-dev \
    libopencv-contrib3.2 libopencv-core3.2 libopencv-objdetect3.2 libopencv-imgproc3.2 \
    opencv-doc

#WORKDIR /usr/src
#RUN git clone --branch 4.2.0 https://github.com/opencv/opencv.git
#RUN git clone --branch 4.2.0 https://github.com/opencv/opencv_contrib.git
#RUN apt install -y 
#
#RUN mkdir opencv/build && \
    # cd opencv/build && \
    # cmake -D CMAKE_BUILD_TYPE=Release \
    #       -D CMAKE_INSTALL_PREFIX=/usr \
    #       -D OPENCV_GENERATE_PKGCONFIG=ON \
    #       -D BUILD_LIST=core,imgproc,dnn,objdetect,video,face \
    #       -D OPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules \
    #       -D WITH_OPENGL=ON \
    #       -S .. -B . && \
    # make -j install || make install

