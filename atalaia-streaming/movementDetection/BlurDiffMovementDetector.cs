using OpenCvSharp;
using System;
using System.Collections.Generic;
using System.Text;

namespace atalaia.streaming.movementDetection
{
    public class BlurDiffMovementDetector
    {
        private Size blurSize = new Size(15, 15);
        private int minArea = 12;
        private Mat lastFrame;

        public MovementAnalysis DetectMovement(Mat originalFrame, out Mat diff)
        {
            var gray = originalFrame.ExecuteOperations(true,
                (i, o) => Cv2.Resize(i, o, new Size(416, 416.0 / originalFrame.Width * originalFrame.Height)),
                (i, o) => Cv2.CvtColor(i, o, ColorConversionCodes.BGR2GRAY),
                (i, o) => Cv2.GaussianBlur(i, o, blurSize, 2, 2));

            try
            {
                if (this.lastFrame != null)
                {
                    var movements = new LinkedList<DetectedMovement>();
                    Point[][] contours;
                    HierarchyIndex[] hierarchy;
                    double maxArea = 0;

                    diff = this.lastFrame.ExecuteOperations(false,
                        (i, o) => Cv2.Absdiff(gray, i, o),
                        (i, o) => Cv2.Threshold(i, o, 15, 255, ThresholdTypes.Binary),
                        (i, o) => Cv2.Dilate(i, o, new Mat(), null, 2));

                    Cv2.FindContours(diff, out contours, out hierarchy, RetrievalModes.External, ContourApproximationModes.ApproxSimple);

                    int idx = 0;

                    foreach (Point[] contour in contours)
                    {
                        double area = Cv2.ContourArea(contour);

                        if (area >= this.minArea)
                        {
                            movements.AddLast(new DetectedMovement()
                            {
                                Contour = contour
                            });
                        }

                        maxArea = Math.Max(area, maxArea);

                        idx++;
                    }

                    return new MovementAnalysis(movements, maxArea);
                }
                else
                {
                    diff = null;
                    return new MovementAnalysis();
                }
            }
            finally
            {
                this.lastFrame = gray;
            }
        }
    }
}
