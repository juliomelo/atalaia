#include "opencv2/tracking.hpp"
#include "CameraAtalaia.hpp"
#include "opencv2/highgui.hpp"
#include "ObjectDetector.hpp"
using namespace cv;

std::set<string> CameraAtalaia::objectOfInterest;

CameraAtalaia::CameraAtalaia(std::string url) : queue(30 * 1000), stream(&this->queue)
{
    if (CameraAtalaia::objectOfInterest.empty())
    {
        CameraAtalaia::objectOfInterest.insert("person");
        CameraAtalaia::objectOfInterest.insert("bicycle");
        CameraAtalaia::objectOfInterest.insert("motorbike");
        CameraAtalaia::objectOfInterest.insert("horse");
        CameraAtalaia::objectOfInterest.insert("backpack");
        CameraAtalaia::objectOfInterest.insert("umbrella");
        CameraAtalaia::objectOfInterest.insert("handbag");
        CameraAtalaia::objectOfInterest.insert("tie");
        CameraAtalaia::objectOfInterest.insert("suitcase");
        CameraAtalaia::objectOfInterest.insert("baseball bat");
        CameraAtalaia::objectOfInterest.insert("baseball glove");
        CameraAtalaia::objectOfInterest.insert("skateboard");
        CameraAtalaia::objectOfInterest.insert("bottle");
        CameraAtalaia::objectOfInterest.insert("cup");
        CameraAtalaia::objectOfInterest.insert("fork");
        CameraAtalaia::objectOfInterest.insert("knife");
        CameraAtalaia::objectOfInterest.insert("spoon");
        CameraAtalaia::objectOfInterest.insert("chair");
        CameraAtalaia::objectOfInterest.insert("sofa");
        CameraAtalaia::objectOfInterest.insert("diningtable");
        CameraAtalaia::objectOfInterest.insert("bed");
        CameraAtalaia::objectOfInterest.insert("tvmonitor");
        CameraAtalaia::objectOfInterest.insert("laptop");
        CameraAtalaia::objectOfInterest.insert("mouse");
        CameraAtalaia::objectOfInterest.insert("remote");
        CameraAtalaia::objectOfInterest.insert("keyboard");
        CameraAtalaia::objectOfInterest.insert("cell phone");
        CameraAtalaia::objectOfInterest.insert("microwave");
        CameraAtalaia::objectOfInterest.insert("oven");
        CameraAtalaia::objectOfInterest.insert("toaster");
        CameraAtalaia::objectOfInterest.insert("sink");
        CameraAtalaia::objectOfInterest.insert("refrigerator");
    }

    this->stream.start(url);
    this->running = true;
    this->thread = new std::thread(this->threadProcess, this);
}

CameraAtalaia::~CameraAtalaia()
{
    this->running = false;
}

void CameraAtalaia::threadProcess(CameraAtalaia *camera)
{
    YoloV3ObjectDetector objectDetector;
    CameraState state = NORMAL;
    int64_t lastPts;
    //Ptr<Tracker> tracker = TrackerBoosting::create();
    Ptr<MultiTracker> multiTracker = cv::MultiTracker::create();
    //VideoWriter writer("teste.mp4", VideoWriter_fourcc('H', '2', '6', '4'), 30);

    do
    {
        FrameQueueItem *frame = camera->queue.pop();

        Mat segmented = frame->mat.clone();
        Scalar color(0, 0, 255);

        multiTracker->update(segmented);
        vector<Rect2d> trackedObjs = multiTracker->getObjects();

        for (int i = 0; i < trackedObjs.size(); i++)
        {
            Rect2d obj = trackedObjs[i];
            rectangle(segmented, obj, Scalar(0, 255, 0));

            std::string label = format("Interesse %d", i);

            int baseLine;
            Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

            int top = max((int)obj.y, labelSize.height);
            rectangle(segmented, Point(obj.x, top - labelSize.height),
                        Point(obj.x + labelSize.width, top + baseLine), Scalar::all(255), FILLED);
            putText(segmented, label, Point(obj.x, top), FONT_HERSHEY_SIMPLEX, 0.5, Scalar());
        }

        for (int i = 0; i < frame->movements.size(); i++)
        {
            polylines(segmented, frame->movements[i].contour, true, Scalar(0, 0, 255));
            // Rect rect = boundingRect(frame->movements[i].contour);
            // float areaThreshold = rect.area() * .9f;
            // bool match = false;

            // if (rect.area() < (frame->mat.cols * frame->mat.rows) * .9f)
            // {
            //     for (int i = 0; i < objects.size(); i++)
            //         if ((Rect2d(rect) & objects[i]).area() >= areaThreshold)
            //         {
            //             match = true;
            //             break;
            //         }

            //     if (!match)
            //         multiTracker->add(TrackerBoosting::create(), segmented, boundingRect(frame->movements[i].contour));
            // }
        }

        switch (state)
        {
        case DETECTED_OBJECT_OF_INTEREST:
        {
            int spent = (frame->packet->pts - lastPts) * frame->time_base.num / frame->time_base.den;
            if (spent < 5)
            {
                camera->record(frame->packet);
                break;
            }
            else
            {
                state = NORMAL;
            }
        }

        case NORMAL:
            list<Rect> rects;
            
            for (int i = 0; i < frame->movements.size(); i++)
            {
                Rect r = boundingRect(frame->movements[i].contour);
                float areaThreshold = r.area() * .5f;
                bool match = false;

                for (int j = 0; j < trackedObjs.size(); j++)
                    if ((Rect2d(r) & trackedObjs[j]).area() >= areaThreshold)
                    {
                        match = true;
                        break;
                    }

                if (!match)
                    rects.push_back(r);
            }

            if (rects.empty())
            {
                cout << "Yeah!\n";
                continue; // às vezes seria bom executar o classificador mesmo sem movimento para detectar objetos parados
            }

            DetectedObjects objects = objectDetector.classifyFromMovements(frame->mat, rects);

            if (!objects.empty())
            {
                bool interested = false;

                for (int i = 0; i < objects.size(); i++)
                {
                    objectDetector.drawObject(frame->mat, objects[i]);
                    objectDetector.drawObject(segmented, objects[i]);
                    float detectedAreaThreshold = objects[i].box.area() * .35f;

                    if (objectOfInterest.find(objects[i].type) != objectOfInterest.end())
                    {
                        Rect rect = objects[i].box;
                        float areaThreshold = rect.area() * .35f;
                        bool match = false;

                        for (int i = 0; i < trackedObjs.size(); i++)
                            if ((Rect2d(rect) & trackedObjs[i]).area() >= min(areaThreshold, detectedAreaThreshold))
                            {
                                match = true;
                                break;
                            }
                            else
                            {
                                Rect2d obj = trackedObjs[i];
                                rectangle(frame->mat, obj, Scalar(0, 255, 0));

                                std::string label = format("Interesse %d", i + 1);

                                int baseLine;
                                Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

                                int top = max((int)obj.y, labelSize.height);
                                rectangle(frame->mat, Point(obj.x, top),
                                          Point(obj.x + labelSize.width, top + baseLine), Scalar::all(255), FILLED);
                                putText(frame->mat, label, Point(obj.x, top), FONT_HERSHEY_SIMPLEX, 0.5, Scalar());
                            }

                        if (!match)
                            multiTracker->add(TrackerKCF::create(), segmented, rect);

                        interested = true;
                    }
                }

                if (interested)
                {
                    vector<AVPacket> packets = camera->stream.getPacketsSince(frame->packet->pts);

                    for (int i = 0; i < packets.size(); i++)
                    {
                        camera->record(&packets[i]);
                        av_packet_unref(&packets[i]);
                    }

                    camera->record(frame->packet);
                    // state = DETECTED_OBJECT_OF_INTEREST;
                    lastPts = frame->packet->pts;
                }

                imshow("objects", frame->mat);

                waitKey((int)(1000.0 / 30));
            }
            break;
        }

        imshow("movement", segmented);

        delete frame;
    } while (camera->running);
}

void CameraAtalaia::record(AVPacket *)
{
}