using OpenCvSharp;
using System;
using System.Collections.Generic;
using System.Text;

namespace atalaia.streaming.movementDetection
{
    public struct DetectedMovement
    {
        public Point[] Contour
        {
            get; set;
        }

        public DetectedMovement(Point[] contour)
        {
            this.Contour = contour;
        }
    }
}
