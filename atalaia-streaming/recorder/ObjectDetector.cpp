// Based on https://github.com/opencv/opencv/blob/master/samples/dnn/object_detection.cpp
#include "ObjectDetector.hpp"

#include <fstream>
#include <sstream>
#include "math.h"
#include <list>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <opencv2/imgproc/imgproc.hpp>
#include "../main.hpp"

using namespace cv;
using namespace dnn;

#define CLASSIFY_FACTOR 8

ObjectDetector::ObjectDetector(string modelPath, string configPath, string classesPath, Size size, float classifyFactor, double confThreshold, double nmsThreshold)
: ClassifierFromMovement(size, classifyFactor, false, confThreshold, nmsThreshold)
{
    ifstream ifs(classesPath.c_str());
    string line;

    while (std::getline(ifs, line))
    {
        this->classes.push_back(line);
    }

    this->net = readNet(modelPath, configPath);
    this->outNames = net.getUnconnectedOutLayersNames();
    this->outLayers = net.getUnconnectedOutLayers();
    this->outLayerType = net.getLayer(outLayers[0])->type;

    this->confThreshold = confThreshold;
    this->nmsThreshold = nmsThreshold;
}

inline void ObjectDetector::preprocess(const Mat &originalFrame, Net &net, Size inpSize)
{
    static Mat blob;
    // Create a 4D blob from a frame.
    if (inpSize.width <= 0)
        inpSize.width = originalFrame.cols;
    if (inpSize.height <= 0)
        inpSize.height = originalFrame.rows;
    
    Mat frame;
    resize(originalFrame, frame, inpSize);
    //blobFromImage(frame, blob, 1.0, inpSize, Scalar(), false, false);
    blobFromImage(frame, blob, 1.0 / 255.0, inpSize, Scalar(), true, false);

#ifdef SHOW_OBJECT_DETECTION_NET
    imshow("net", frame);
    waitKey(25);
#endif

    // Run a model.
    this->net.setInput(blob);
    if (this->net.getLayer(0)->outputNameToIndex("im_info") != -1) // Faster-RCNN or R-FCN
    {
        resize(frame, frame, inpSize);
        Mat imInfo = (Mat_<float>(1, 3) << inpSize.height, inpSize.width, 1.6f);
        this->net.setInput(imInfo, "im_info");
    }
}

std::vector<DetectedObject> ObjectDetector::postprocess(Mat &frame, const std::vector<Mat> &outs, Net &net)
{
    DetectedObjects results;
    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<Rect> boxes;

    if (this->outLayerType == "DetectionOutput")
    {
        // Network produces output blob with a shape 1x1xNx7 where N is a number of
        // detections and an every detection is a vector of values
        // [batchId, classId, confidence, left, top, right, bottom]
        for (size_t k = 0; k < outs.size(); k++)
        {
            float *data = (float *)outs[k].data;
            for (size_t i = 0; i < outs[k].total(); i += 7)
            {
                float confidence = data[i + 2];
                if (confidence > confThreshold)
                {
                    int left = (int)data[i + 3];
                    int top = (int)data[i + 4];
                    int right = (int)data[i + 5];
                    int bottom = (int)data[i + 6];
                    int width = right - left + 1;
                    int height = bottom - top + 1;
                    if (width <= 2 || height <= 2)
                    {
                        left = (int)(data[i + 3] * frame.cols);
                        top = (int)(data[i + 4] * frame.rows);
                        right = (int)(data[i + 5] * frame.cols);
                        bottom = (int)(data[i + 6] * frame.rows);
                        width = right - left + 1;
                        height = bottom - top + 1;
                    }

                    classIds.push_back((int)(data[i + 1]) - 1); // Skip 0th background class id.
                    boxes.push_back(Rect(left, top, width, height));
                    confidences.push_back(confidence);
                }
            }
        }
    }
    else if (outLayerType == "Region")
    {
        for (size_t i = 0; i < outs.size(); ++i)
        {
            // Network produces output blob with a shape NxC where N is a number of
            // detected objects and C is a number of classes + 4 where the first 4
            // numbers are [center_x, center_y, width, height]
            float *data = (float *)outs[i].data;
            for (int j = 0; j < outs[i].rows; ++j, data += outs[i].cols)
            {
                Mat scores = outs[i].row(j).colRange(5, outs[i].cols);
                Point classIdPoint;
                double confidence;
                minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
                if (confidence > confThreshold)
                {
                    int centerX = (int)(data[0] * frame.cols);
                    int centerY = (int)(data[1] * frame.rows);
                    int width = (int)(data[2] * frame.cols);
                    int height = (int)(data[3] * frame.rows);
                    int left = centerX - width / 2;
                    int top = centerY - height / 2;

                    classIds.push_back(classIdPoint.x);
                    confidences.push_back((float)confidence);
                    boxes.push_back(Rect(left, top, width, height));
                }
            }
        }
    }
    else
        CV_Error(Error::StsNotImplemented, "Unknown output layer type: " + outLayerType);

    vector<DetectedObject> objects(boxes.size());

    for (size_t i = 0; i < boxes.size(); ++i)
        objects[i] = DetectedObject(this->classes[classIds[i]], confidences[i], boxes[i]);

    return objects;
}

vector<DetectedObject> ObjectDetector::classify(Mat &mat)
{
    preprocess(mat, net, this->size);

    std::vector<Mat> outs;
    net.forward(outs, outNames);

    return postprocess(mat, outs, net);
}

YoloV3ObjectDetector::YoloV3ObjectDetector(string modelPath, string configPath, string classesPath, int width, int height) : ObjectDetector(modelPath, configPath, classesPath, Size(width, height), CLASSIFY_FACTOR)
{
}

void ObjectDetector::drawObject(Mat &frame, DetectedObject &obj)
{
    rectangle(frame, obj.box, Scalar(0, 255, 0));

    std::string label = format("%s: %.2f", obj.type.c_str(), obj.confidence);

    int baseLine;
    Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

    int top = max(obj.box.y, labelSize.height);
    rectangle(frame, Point(obj.box.x, top - labelSize.height),
              Point(obj.box.x + labelSize.width, top + baseLine), Scalar::all(255), FILLED);
    putText(frame, label, Point(obj.box.x, top), FONT_HERSHEY_SIMPLEX, 0.5, Scalar());
}


Rect ObjectDetector::getBox(DetectedObject &item)
{
    return item.box;
}

float ObjectDetector::getConfidence(DetectedObject &item)
{
    return item.confidence;
}

void ObjectDetector::translate(DetectedObject &item, int dx, int dy)
{
    item.box.x += dx;
    item.box.y += dy;
}
