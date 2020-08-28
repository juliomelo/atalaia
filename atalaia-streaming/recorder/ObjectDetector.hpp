#pragma once

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include "MovementDetector.hpp"
#include <list>
#include "../util/ClassifierFromMovement.hpp"

using namespace cv;
using namespace dnn;

class DetectedObject
{
    public:
        DetectedObject()
        {
            this->confidence = 0;
            this->missCount = 0;
            this->id = 0;
        }

        DetectedObject(const DetectedObject &obj)
        {
            this->type = obj.type;
            this->confidence = obj.confidence;
            this->box = obj.box;
            this->missCount = obj.missCount;
            this->id = obj.id;
        }

        DetectedObject(std::string type, float confidence, Rect box)
        {
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

class ObjectDetector : public ClassifierFromMovement<DetectedObject>
{
    public:
        ObjectDetector(string modelPath, string configPath, string classesPath, Size size, float classifyFactor, double confThreshold = .5, double nmsThreshold = .25);

        std::vector<DetectedObject> classify(Mat &mat);
        
        void drawObject(Mat &frame, DetectedObject &obj);

    protected:
        Net net;
        std::vector<std::string> classes;
        std::vector<String> outNames;
        std::vector<int> outLayers;
        std::string outLayerType;
        double confThreshold;
        double nmsThreshold;

        void preprocess(const Mat &frame, Net &net, Size inpSize);
        std::vector<DetectedObject> postprocess(Mat &frame, const std::vector<Mat> &outs, Net &net);

        Rect getBox(DetectedObject &item);
        float getConfidence(DetectedObject &item) ;
        void translate(DetectedObject &item, int dx, int dy);
};

class YoloV3ObjectDetector : public ObjectDetector
{
    public:
        YoloV3ObjectDetector(string modelPath = "data/yolo/yolov3-tiny.weights", string configPath = "data/yolo/yolov3-tiny.cfg", string classesPath = "data/yolo/coco.names", int width = 416, int height = 416); // yolov3 - tiny uses 416x416 images
};