using OpenCvSharp;
using OpenCvSharp.Dnn;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Linq;

namespace atalaia.streaming.objectDetection
{
    public class YoloV3
    {
        private Net net;
        private string[] outputNames;
        private Size imgSize;
        private double confidenceThreshold = .75;
        private string[] classNames;
        private Mat[] outs;

        public YoloV3()
        {
            this.net = CvDnn.ReadNetFromDarknet("/data/yolo/yolov3-tiny.cfg", "/data/yolo/yolov3-tiny.weights");
            this.imgSize = new Size(416, 416); // yolov3-tiny uses 416x416 images
            this.outputNames = net.GetUnconnectedOutLayersNames();
            this.classNames = File.ReadLines("/data/yolo/coco.names").ToArray();
            //net.SetPreferableTarget(Net.Target.OPENCL);

            outs = new Mat[this.outputNames.Length];

            for (int i = 0; i < this.outputNames.Length; i++)
            {
                outs[i] = new Mat();
            }
        }

        public LinkedList<DetectedObject> ClassifyImg(Mat img)
        {
            var result = new LinkedList<DetectedObject>();

            using (var blob = CvDnn.BlobFromImage(img, 1.0 / 255, this.imgSize, default(Scalar), true, false))
            {
                // blobFromImage(frame, blob, 1.0, inpSize, Scalar(), swapRB, false, CV_8U);

                net.SetInput(blob);
                net.Forward(outs, this.outputNames);

                foreach (Mat mat in outs)
                {
                    float[] data;

                    if (!mat.GetArray(out data))
                        throw new Exception("Failed to get array from yolo output.");

                    // According to https://github.com/opencv/opencv/blob/1f2b2c52422813188bc5cbcc5d312c89e41786b2/samples/dnn/object_detection.cpp#L368-L394
                    // Network produces output blob with a shape NxC where N is a number of
                    // detected objects and C is a number of classes + 4 where the first 4
                    // numbers are [center_x, center_y, width, height]
                    for (int i = 0, j = 0; j < mat.Rows; ++j, i += mat.Cols)
                    {
                        using (Mat scores = mat.Row(j).ColRange(5, mat.Cols))
                        {
                            Point classIdPoint, minLoc;
                            double confidence, minVal;

                            Cv2.MinMaxLoc(scores, out minVal, out confidence, out minLoc, out classIdPoint);

                            if (confidence > this.confidenceThreshold)
                            {
                                int centerX = (int)(data[i + 0] * img.Cols);
                                int centerY = (int)(data[i + 1] * img.Rows);
                                int width = (int)(data[i + 2] * img.Cols);
                                int height = (int)(data[i + 3] * img.Rows);
                                int left = centerX - width / 2;
                                int top = centerY - height / 2;

                                result.AddLast(new DetectedObject(this.classNames[classIdPoint.X], confidence, top, left, width, height));
                            }
                        }
                    }
                }
            }

            return result;
        }
    }
}
