// Based on https://github.com/opencv/opencv/blob/master/samples/dnn/object_detection.cpp
#include "ObjectDetector.hpp"

#include <fstream>
#include <sstream>
#include "math.h"
#include <list>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <opencv2/imgproc/imgproc.hpp>

using namespace cv;
using namespace dnn;

ObjectDetector::ObjectDetector(string modelPath, string configPath, string classesPath, double confThreshold, double nmsThreshold)
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

inline void ObjectDetector::preprocess(const Mat &frame, Net &net, Size inpSize)
{
    static Mat blob;
    // Create a 4D blob from a frame.
    if (inpSize.width <= 0)
        inpSize.width = frame.cols;
    if (inpSize.height <= 0)
        inpSize.height = frame.rows;
    //blobFromImage(frame, blob, 1.0, inpSize, Scalar(), false, false);
    blobFromImage(frame, blob, 1.0 / 255.0, inpSize, Scalar(), true, false);

    // Run a model.
    this->net.setInput(blob);
    if (this->net.getLayer(0)->outputNameToIndex("im_info") != -1) // Faster-RCNN or R-FCN
    {
        resize(frame, frame, inpSize);
        Mat imInfo = (Mat_<float>(1, 3) << inpSize.height, inpSize.width, 1.6f);
        this->net.setInput(imInfo, "im_info");
    }
}

void translate(DetectedObjects &d, int dx, int dy)
{
    for (int i = 0; i < d.size(); i++)
    {
        d[i].box.x += dx;
        d[i].box.y += dy;
    }
}

DetectedObjects ObjectDetector::postprocess(Mat &frame, const std::vector<Mat> &outs, Net &net)
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

    std::vector<int> indices;
    NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);

    DetectedObjects objects;

    for (size_t i = 0; i < indices.size(); ++i)
        objects.push_back(DetectedObject(this->classes[classIds[i]], confidences[i], boxes[i]));

    return objects;
}

DetectedObjects ObjectDetector::classify(Mat &mat)
{
    imshow("Classify", mat);
    cv::waitKey((int)(1000.0 / 30));

    preprocess(mat, net, this->size);

    std::vector<Mat> outs;
    net.forward(outs, outNames);

    return postprocess(mat, outs, net);
}

YoloV3ObjectDetector::YoloV3ObjectDetector(string modelPath, string configPath, string classesPath, int width, int height) : ObjectDetector(modelPath, configPath, classesPath)
{
    this->size = Size(width, height);
}

DetectedObjects ObjectDetector::classifyFromMovements(Mat &mat, list<Rect> rects)
{
    if (!rects.empty() && (mat.cols > this->size.width || mat.rows > this->size.height))
    {
        DetectedObjects result;

        do
        {
            uint minX = ~0, minY = ~0, maxX = 0, maxY = 0;

            for (list<Rect>::iterator it = rects.begin(); it != rects.end(); ++it)
            {
                Rect r = *it;

                if (r.width >= this->size.width || r.height >= this->size.height)
                {
                    // The movement area is bigger than desired size, so let's classify it directly...
                    Mat crop = mat(r);
                    DetectedObjects d = this->classify(crop);
                    translate(d, r.x, r.y);
                    result.insert(result.end(), d.begin(), d.end());
                    it = rects.erase(it);
                } else {
                    minX = minX < r.x ? minX : r.x;
                    minY = minY < r.y ? minY : r.y;
                }
            }

            if (!rects.empty())
            {
                uint right = minX + this->size.width, bottom = minY + this->size.height;

                // Let's classify every object in the viewport.
                for (list<Rect>::iterator it = rects.begin(); it != rects.end();)
                {
                    Rect r = *it;

                    if (r.x >= minX && r.x + r.width <= right && r.y >= minY && r.y + r.height <= bottom)
                    {
                        maxX = maxX > r.x + r.width ? maxX : r.x + r.width;
                        maxY = maxY > r.y + r.height ? maxY : r.y + r.height;
                        it = rects.erase(it);
                    } else {
                        ++it;
                    }
                }

                // Let's center the image
                Rect r(minX - (right - maxX) / 2, minY - (bottom - maxY) / 2, this->size.width, this->size.height);

                if (r.x < 0)
                    r.x = 0;
                else if (r.x + r.width > mat.cols)
                    r.x = mat.cols - r.width;

                if (r.y < 0)
                    r.y = 0;
                else if (r.y + r.height > mat.rows)
                    r.y = mat.rows - r.height;

                Mat crop = mat(r);
                DetectedObjects d = this->classify(crop);
                translate(d, r.x, r.y);
                result.insert(result.end(), d.begin(), d.end());
            }
        } while (!rects.empty());

        return result;
    }

    return ObjectDetector::classify(mat);
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
