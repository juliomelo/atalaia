using OpenCvSharp;
using System;
using System.Collections.Generic;
using System.Text;

namespace atalaia.streaming.movementDetection
{
    public class BufferedBlurDiffMovementDetector
    {
        private BlurDiffMovementDetector movementDetector = new BlurDiffMovementDetector();
        private Mat diffRef;
        private DateTime diffRefTs;
        private List<DetectedMovementWithFrame> pastFrames = new List<DetectedMovementWithFrame>(30 * 5);

        public DetectedMovementWithFrame[] DetectMovement(FrameData frameData)
        {
            Mat diff;
            var analysis = movementDetector.DetectMovement(frameData.Frame, out diff);

            if (analysis.DetectedMovement)
            {
                pastFrames.Add(new DetectedMovementWithFrame()
                {
                    FrameData = frameData,
                    Analysis = analysis
                });

                if (diffRef == null)
                {
                    diffRef = diff;
                    diffRefTs = frameData.Ts;
                }
                else
                {
                    var andDiff = diffRef.ExecuteOperations(true,
                        (i, o) => Cv2.BitwiseAnd(i, diff, o),
                        (i, o) => Cv2.Dilate(i, o, new Mat(), null, 4),
                        (i, o) => Cv2.Erode(i, o, new Mat(), null, 2));

                    //Cv2.ImShow("img", frameData.Frame);
                    //Cv2.ImShow("firstDiff", diffRef);
                    //Cv2.ImShow("andDiff", andDiff);
                    //Cv2.WaitKey(50);

                    Point[][] contours;
                    HierarchyIndex[] hierarchy;

                    Cv2.FindContours(diff, out contours, out hierarchy, RetrievalModes.External, ContourApproximationModes.ApproxSimple);

                    if (contours.Length == 0)
                    {
                        clear(true);
                    }
                    else if ((frameData.Ts - pastFrames[0].FrameData.Ts).TotalSeconds >= 1.5)
                    {
                        DetectedMovementWithFrame[] result = pastFrames.ToArray();
                        clear(false);
                        return result;
                    }
                    else if ((frameData.Ts - diffRefTs).TotalMilliseconds >= 150)
                    {
                        diffRef.Dispose();
                        diffRef = diff;
                        diffRefTs = frameData.Ts;
                    }
                }
            }
            else
            {
                clear(true);
            }

            return new DetectedMovementWithFrame[0];
        }

        private void clear(bool dispose)
        {
            if (dispose)
            {
                this.pastFrames.ForEach(p => p.FrameData.Frame.Dispose());
            }
            this.pastFrames.Clear();

            if (this.diffRef != null)
            {
                this.diffRef.Dispose();
            }
            this.diffRef = null;
        }
    }

    public struct DetectedMovementWithFrame
    {
        public FrameData FrameData { get; set; }
        public MovementAnalysis Analysis { get; set; }
    }
}
