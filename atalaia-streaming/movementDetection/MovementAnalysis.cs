using System;
using System.Collections.Generic;
using System.Text;

namespace atalaia.streaming.movementDetection
{
    public struct MovementAnalysis
    {
        public ICollection<DetectedMovement> Movements { get; set; }
        public double MaxAreaDiff { get; set; }

        public bool DetectedMovement
        {
            get { return this.Movements != null && this.Movements.Count > 0; }
        }
    }
}
