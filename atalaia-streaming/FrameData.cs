using OpenCvSharp;
using System;
using System.Collections.Generic;
using System.Text;

namespace atalaia.streaming.movementDetection
{
    public struct FrameData
    {
        public FrameData(Mat frame)
        {
            this.Frame = frame;
            this.Ts = DateTime.Now;
        }

        public Mat Frame
        {
            get; set;
        }

        public DateTime Ts
        {
            get; set;
        }
    }
}
