#pragma once

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include "MovementDetector.hpp"
#include <list>

using namespace cv;
using namespace dnn;

class DetectedObject
{
    public:
        DetectedObject(std::string type, float confidence, Rect box){
            this->type = type;
            this->confidence = confidence;
            this->box = box;
            this->missCount = 0;
            this->id = 0;
        }

        std::string type;
        float confidence;
        Rect box;
        int missCount;
        unsigned int id;
};

typedef std::vector<DetectedObject> DetectedObjects;

class ObjectDetector
{
    public:
        ObjectDetector(string modelPath, string configPath, string classesPath, double confThreshold = .5, double nmsThreshold = .25);

        std::vector<DetectedObject> classify(Mat &mat, bool runNMS = true);
        std::vector<DetectedObject> classifyFromMovements(Mat &mat, list<Rect> movements);
        
        void drawObject(Mat &frame, DetectedObject &obj);

    protected:
        Size size;
        Net net;
        std::vector<std::string> classes;
        std::vector<String> outNames;
        std::vector<int> outLayers = net.getUnconnectedOutLayers();
        std::string outLayerType = net.getLayer(outLayers[0])->type;
        double confThreshold;
        double nmsThreshold;

        void preprocess(const Mat &frame, Net &net, Size inpSize);
        std::vector<DetectedObject> postprocess(Mat &frame, const std::vector<Mat> &outs, Net &net, bool runNMS);
};

class YoloV3ObjectDetector : public ObjectDetector
{
    public:
        YoloV3ObjectDetector(string modelPath = "/data/yolo/yolov3-tiny.weights", string configPath = "/data/yolo/yolov3-tiny.cfg", string classesPath = "/data/yolo/coco.names", int width = 416, int height = 416); // yolov3 - tiny uses 416x416 images
};