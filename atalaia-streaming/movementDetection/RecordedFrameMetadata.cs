using System;
using System.Collections.Generic;
using System.Text;

namespace atalaia.streaming.movementDetection
{
    public struct RecordedFrameMetadata
    {
        public DateTime Ts { get; set; }

        public IEnumerable<DetectedMovement> Movements { get; set; }
    }
}
