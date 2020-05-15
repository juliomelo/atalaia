#include "ObjectRecorder.hpp"
#include <opencv2/tracking/tracker.hpp>
#include <opencv2/highgui.hpp>
#include <set>
#include <stdio.h>

using namespace cv;

#define matchAreaMatrix(a, b, c) matchArea[a * nTracked * 2 + b * 2 + c]

void trackObjects(DetectedObjects newObjects, vector<DetectedObject> &lastObjects, MultiTracker *&tracker, Mat &mat)
{
    vector<Rect2d> trackedObjects = tracker->getObjects();
    int nNew = newObjects.size();
    int nTracked = trackedObjects.size();
    double matchArea[nNew * nTracked * 2]; // new x tracked x (new area | last area)

    memset(matchArea, 0, 2 * nNew * nTracked * sizeof(double));

    for (int i = 0; i < nNew; i++) {
        Rect doRect = newObjects[i].box;
        Rect2d doRect2d(doRect);
        double doArea = doRect.area();

        for (int j = 0; j < nTracked; j++) {
            double toArea = trackedObjects[j].area();
            matchAreaMatrix(i, j, 0) = (doRect2d & trackedObjects[j]).area() / (doArea > toArea ? doArea : toArea);

            double loArea = lastObjects[j].box.area();
            matchAreaMatrix(i, j, 1) = (doRect & lastObjects[j].box).area() / (doArea > loArea ? doArea : loArea);
        }
    }

    bool matchesNew[nNew], matchesTracked[nTracked];
    memset(matchesNew, 0, sizeof(bool) * nNew);
    memset(matchesTracked, 0, sizeof(bool) * nTracked);
    int nMatches = 0;
    bool iterate;

    do
    {
        iterate = false;
        int bestForNew[nNew];

        for (int iNew = 0; iNew < nNew; iNew++)
        {
            int best = -1;

            for (int iObj = 0; iObj < nTracked; iObj++)
                if (matchAreaMatrix(iNew, iObj, 0) > matchAreaMatrix(iNew, best, 0) && newObjects[iNew].type == lastObjects[iObj].type)
                    best = iNew;

            bestForNew[iNew] = best;
        }

        int bestForTracked[nTracked];

        for (int iObj = 0; iObj < nTracked; iObj++)
        {
            int best = -1;

            for (int iNew = 0; iNew < nNew; iNew++)
                if (matchAreaMatrix(iNew, iObj, 0) > matchAreaMatrix(best, iObj, 0) && newObjects[iNew].type == lastObjects[iObj].type)
                    best = iNew;

            bestForTracked[iObj] = best;

            if (best != -1 && bestForNew[bestForTracked[iObj]] == iObj)
            {
                matchesNew[best] = true;
                matchesTracked[iObj] = true;
                lastObjects[iObj].missCount = 0;
                nMatches++;
                lastObjects[iObj].box = newObjects[best].box;

                if (lastObjects[iObj].confidence < newObjects[best].confidence)
                    lastObjects[iObj].confidence = newObjects[best].confidence;

                iterate = nMatches < nNew;
            }
        }
    } while (iterate);

    bool removedSome = false;

    // Remove lost objects.
    for (int i = nTracked - 1; i >= 0; i--)
        if (!matchesTracked[i] && ++(lastObjects[i].missCount) > 30)
        {
            lastObjects.erase(lastObjects.begin() + i);
            removedSome = true;
        }
        else
            lastObjects[i].box = trackedObjects[i];

    if (removedSome) {
        delete tracker; 
        tracker = new MultiTracker();
        tracker->update(mat);

        for (int i = 0; i < lastObjects.size(); i++)
            tracker->add(TrackerKCF::create(), mat, lastObjects[i].box);
    }

    for (int i = 0; i < nNew; i++)
    {
        if (!matchesNew[i])
        {
            for (int j = 0; j < nTracked; j++)
            {
                if (!matchesTracked[j] && matchAreaMatrix(i, j, 0) > .95)
                {
                    matchesNew[i] = true;
                    matchesTracked[j] = true;

                    lastObjects[j].box = newObjects[i].box;

                    if (lastObjects[j].confidence < newObjects[i].confidence)
                        lastObjects[j].confidence = newObjects[i].confidence;

                    break;
                }
            }

            if (!matchesNew[i] && newObjects[i].confidence >= .6)
            {
                lastObjects.push_back(newObjects[i]);
                tracker->add(TrackerKCF::create(), mat, lastObjects[i].box);
            }
        }
    }
}

void ObjectRecorder::process(string file)
{
    cout << "Processing " << file << "\n";

    std::set<string> objectTypes;

    {
        MotionRecordReader reader(file);
        FrameQueueItem *frame = NULL;
        DetectedMovements *movements;
        MultiTracker *multiTracker = NULL;
        DetectedObjects knownObjects;
        unsigned int objectCount = 0;

        while (reader.readNext(frame, movements))
        {
            list<Rect> rects(movements->begin(), movements->end());
            DetectedObjects newObjects = objectDetector.classifyFromMovements(frame->mat, rects);

            if (multiTracker == NULL && !newObjects.empty())
            {
                multiTracker = new MultiTracker();
                multiTracker->update(frame->mat);
            }

            if (multiTracker != NULL)
            {
                multiTracker->update(frame->mat);
                trackObjects(newObjects, knownObjects, multiTracker, frame->mat);
            
                for (int i = 0; i < knownObjects.size(); i++) {
                    DetectedObject *obj = &knownObjects[i];

                    if (obj->id == 0)
                        obj->id = ++objectCount;

                    rectangle(frame->mat, obj->box, obj->missCount > 0 ? Scalar(0, 0, 255) : Scalar(0, 255, 0));

                    std::string label = format("Obj %d: %s (c: %f; m: %d)", obj->id, obj->type.c_str(), obj->confidence, obj->missCount);
                    cout << label << " Rect: [" << obj->box.x << ", " << obj->box.y << "; " << obj->box.x + obj->box.width << ", " << obj->box.y + obj->box.height << "]\n";

                    int baseLine;
                    Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

                    int top = std::max((int)obj->box.y, labelSize.height);
                    rectangle(frame->mat, Point(obj->box.x, top),
                                Point(obj->box.x + labelSize.width, top + baseLine), Scalar::all(255), FILLED);
                    putText(frame->mat, label, Point(obj->box.x, top), FONT_HERSHEY_SIMPLEX, 0.5, Scalar());

                    if (objectTypes.count(obj->type) == 0)
                    {
                        objectTypes.insert(obj->type);
                        this->notifier->notify(file, NotifyEvent::OBJECT, obj->type);
                    }
                }

                if (knownObjects.size() > 0) {
                    cout << "---" << "\n";
                    Mat show;
                    resize(frame->mat, show, Size(640, 480));
                    imshow("objects", show);
                    //imshow("objects", frame->mat);
                    waitKey(25);
                }
            }

            delete frame;
            delete movements;
        }
    }

    if (objectTypes.size() == 0)
    {
        string mp4 = file + ".mp4";
        string movements = file + ".movements";

        remove(mp4.c_str());
        remove(movements.c_str());
    }

    cout << "Processed " << file << "\n";
}
