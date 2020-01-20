using System;
using System.Collections.Generic;
using System.Text;

namespace atalaia.streaming.movementDetection
{
    struct ProcessedFrame
    {
        public FrameData FrameData { get; set; }
        public ICollection<DetectedMovement> Movements { get; set; }
    }
}
