using OpenCvSharp;
using System;
using System.Collections.Generic;
using System.Text;

namespace atalaia.streaming.movementDetection
{
    public struct MovementAnalysis
    {
        public ICollection<DetectedMovement> Movements { get; private set; }
        public double MaxAreaDiff { get; private set; }

        public bool DetectedMovement
        {
            get { return this.Movements != null && this.Movements.Count > 0; }
        }

        public MovementAnalysis(ICollection<DetectedMovement> movements, double maxAreaDiff)
        {
            this.Movements = movements;
            this.MaxAreaDiff = maxAreaDiff;
        }
    }
}
