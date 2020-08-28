#pragma once

#include <opencv2/imgproc/imgproc.hpp>
#include <list>

#include <sstream>
#include "math.h"
#include <iostream>
#include <opencv2/dnn.hpp>

template <class TResult>
class ClassifierFromMovement
{
    protected:
        cv::Size size;
        float classifyFactor;
        bool allowFullscreen;
        double confThreshold;
        double nmsThreshold;

        virtual cv::Rect getBox(TResult &item) = 0;
        virtual float getConfidence(TResult &item) = 0;
        virtual void translate(TResult &item, int dx, int dy) = 0;

    public:
        ClassifierFromMovement(cv::Size size, float classifyFactor, bool allowFullscreen = true, 
            double confThreshold = .5, double nmsThreshold = .25) : size(size), classifyFactor(classifyFactor),
            allowFullscreen(allowFullscreen), confThreshold(confThreshold), nmsThreshold(nmsThreshold) {};

        virtual std::vector<TResult> classify(cv::Mat &mat) = 0;

        std::vector<TResult> classifyFromMovements(cv::Mat &mat, std::list<cv::Rect> rects)
        {
            std::vector<TResult> resultVector;

            if (rects.empty())
            {
                return resultVector;
            }

            if (mat.cols > this->size.width || mat.rows > this->size.height)
            {
                list<TResult> preResult;
                bool classifyFullscreen = false;
                float factor = 1 / (float) this->classifyFactor / (this->size.width / (float) mat.cols);

                if (this->allowFullscreen)
                {
                    Size fullscreenThreshold = Size((int)(size.width * factor), (int)(size.height * factor));

                    for (list<Rect>::iterator it = rects.begin(); it != rects.end();)
                    {
                        Rect r = *it;

                        if (r.width >= fullscreenThreshold.width || r.height >= fullscreenThreshold.height)
                        {
                            it = rects.erase(it);
                            classifyFullscreen = true;
                        } else
                            ++it;
                    }

                    if (classifyFullscreen)
                    {
                        vector<TResult> d = this->classify(mat);
                        preResult.insert(preResult.end(), d.begin(), d.end());
                    }
                }

                float ratio = size.width / (float) size.height;
                float invertedRatio = size.height / (float) size.width;
                uint maxSize = std::min(size.width, size.height);

                // Classify smaller rectangles
                while (!rects.empty())
                {
                    uint x1 = ~0, y1 = ~0;

                    for (list<Rect>::iterator it = rects.begin(); it != rects.end(); ++it)
                    {
                        Rect r = *it;

                        x1 = x1 < r.x ? x1 : r.x;
                        y1 = y1 < r.y ? y1 : r.y;
                    }

                    uint maxX2 = x1 + maxSize;
                    uint maxY2 = y1 + maxSize;
                    uint x2 = 0, y2 = 0;

                    // Let's classify every object in the viewport.
                    for (list<Rect>::iterator it = rects.begin(); it != rects.end();)
                    {
                        Rect r = *it;

                        if (r.x >= x1 && r.x + r.width <= maxX2 && r.y >= y1 && r.y + r.height <= maxY2)
                        {
                            x2 = std::max(x2, (uint) r.x + r.width);
                            y2 = std::max(y2, (uint) r.y + r.height);
                            it = rects.erase(it);
                        } else {
                            ++it;
                        }
                    }

                    if (x2 == 0)
                    {
                        if (!classifyFullscreen)
                        {
                            vector<TResult> d = this->classify(mat);
                            preResult.insert(preResult.end(), d.begin(), d.end());
                        }

                        break;
                    }

                    // Let's center the image
                    float newRatio = (x2 - x1) / (float) (y2 - y1);
                    bool invert = ratio >= 1 && newRatio < 1 || ratio < 1 && newRatio >= 1;
                    uint width = std::max((uint) size.width, invert ? (uint) (ratio * (y2 - y1)) : x2 - x1);
                    uint height = std::max((uint) size.height, invert ? y2 - y1 : (uint) (invertedRatio * width));

                    Rect r(x1 - std::max((uint) 0, (uint) width - (x2 - x1) /* mudar aqui pelo tamanho desejado */) / 2,
                        y1 - std::max((uint) 0, (uint) height - (y2 - y1) /* mudar aqui pela altura desejada */) / 2,
                        width, height);

                    if (r.x < 0)
                        r.x = 0;
                    else if (r.x + r.width > mat.cols)
                        r.x = mat.cols - r.width;

                    if (r.y < 0)
                        r.y = 0;
                    else if (r.y + r.height > mat.rows)
                        r.y = mat.rows - r.height;

                    Mat crop = mat(r);
                    vector<TResult> d = this->classify(crop);

                    for (int i = 0; i < d.size(); i++)
                    {
                        TResult item = d[i];
                        this->translate(item, r.x, r.y);
                        preResult.push_back(item);
                    }
                }

                resultVector = vector<TResult>(preResult.begin(), preResult.end());
            }
            else
            {
                resultVector = this->classify(mat);
            }
            
            if (resultVector.empty())
            {
                return resultVector;
            }

            std::vector<Rect> boxes;
            std::vector<float> confidences;
            std::vector<int> indices;

            for (int i = 0; i < resultVector.size(); i++)
            {
                TResult obj = resultVector[i];
                boxes.push_back(this->getBox(obj));
                confidences.push_back(this->getConfidence(obj));
                
            }
            
            dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);

            vector<TResult> newResult(indices.size());

            for (size_t i = 0; i < indices.size(); ++i)
            {
                int idx = indices[i];
                cout << ": Found: " << idx << " " << resultVector[idx].type << " " << resultVector[idx].confidence << "\n";

                newResult.push_back(resultVector[indices[i]]);
            }

            return newResult;
        }
};

