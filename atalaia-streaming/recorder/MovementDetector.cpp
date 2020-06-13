#include "MovementDetector.hpp"
#include "opencv2/highgui.hpp"

// Based on https://docs.opencv.org/3.2.0/dd/d9d/segment_objects_8cpp-example.html
MovementDetector::MovementDetector()
{
    this->bgsubtractor = createBackgroundSubtractorMOG2(500, 16 /*48*/, false);
    this->bgsubtractor->setShadowValue(0);
    // this->bgsubtractor = cv::bgsegm::createBackgroundSubtractorCNT();
    // this->bgsubtractor = cv::bgsegm::createBackgroundSubtractorGSOC();
    this->update_bg_model = true;
}

DetectedMovements MovementDetector::refineSegments(const Mat &img, Mat &mask, /*Mat &dst,*/ double scale)
{
    int niters = 3;
    vector<vector<Point>> contours;
    vector<Vec4i> hierarchy;
    Mat temp;
    dilate(mask, temp, Mat(), Point(-1, -1), niters);
    erode(temp, temp, Mat(), Point(-1,-1), niters*2);
    dilate(temp, temp, Mat(), Point(-1,-1), niters);
    findContours( temp, contours, hierarchy, RETR_CCOMP, CHAIN_APPROX_SIMPLE );
    
    if( contours.size() == 0 )
        return vector<DetectedMovement>();

    // iterate through all the top-level contours,
    // draw each connected component with its own random color
    int idx = 0, largestComp = 0;
    double maxArea = 0;
    double areaThreshold = 54 * 54;

    DetectedMovements result;

    for (; idx >= 0; idx = hierarchy[idx][0])
    {
        const vector<Point>& c = contours[idx];
        double area = fabs(contourArea(Mat(c)));

        if (area * scale >= areaThreshold)
        {
            vector<Point> scaledContour;

            for (int i = 0; i < c.size(); i++)
                scaledContour.push_back(c[i] * scale);

            result.push_back(DetectedMovement(scaledContour));
        }
    }

    return result;
}

DetectedMovements MovementDetector::detectMovement(const Mat &frame)
{
    Mat resized, blurred;
    const int newSize = 700;

    if (newSize < frame.cols)
        cv::resize(frame, resized, Size(newSize, (int)((float) newSize / frame.cols * frame.rows)));
    else
        resized = Mat(frame);

    cv::blur(resized, blurred, Size(newSize / 200, newSize / 200));

    bgsubtractor->apply(blurred, bgmask, update_bg_model ? -1 : 0);
    return refineSegments(frame, bgmask, /*out_frame,*/ frame.cols / (float)resized.cols);
}