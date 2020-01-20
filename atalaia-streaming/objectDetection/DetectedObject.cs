using System;
using System.Collections.Generic;
using System.Text;

namespace atalaia.streaming.objectDetection
{
    public class DetectedObject
    {
        public DetectedObject(string name, double confidence, int top, int left, int width, int height)
        {
            this.Name = name;
            this.Confidence = confidence;
            this.Top = top;
            this.Left = left;
            this.Width = width;
            this.Height = height;
        }

        public string Name { get; set; }
        public double Confidence { get; set; }
        public int Top { get; set; }
        public int Left { get; set; }
        public int Width { get; set; }
        public int Height { get; set; }
    }
}
