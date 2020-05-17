#ifndef MOVEMENT_DETECTOR_H
#define MOVEMENT_DETECTOR_H

#include "opencv2/imgproc.hpp"
// #include "opencv2/video/background_segm.hpp"
#include "opencv2/bgsegm.hpp"
#include <stdio.h>
#include <string>

using namespace std;
using namespace cv;
using namespace cv::bgsegm;

class DetectedMovement
{
    public:
        DetectedMovement()
        { }
        
        DetectedMovement(vector<Point> &contour)
        {
            this->contour = contour;
        }

        vector<Point> contour;

        operator Rect() const {
            return cv::boundingRect(this->contour);
        }
};

typedef std::vector<DetectedMovement> DetectedMovements;

class MovementDetector
{
    public:
        MovementDetector();
        DetectedMovements detectMovement(const Mat& frame);

    private:
        Ptr<BackgroundSubtractor> bgsubtractor;
        Mat bgmask;
        bool update_bg_model;

        DetectedMovements refineSegments(const Mat &img, Mat &mask, /*Mat &dst,*/ double scale = 1);
};

#endif