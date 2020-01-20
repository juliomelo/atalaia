using System;
using System.Collections.Generic;
using System.Text;

namespace atalaia.streaming.movementDetection
{
    public struct MovementRecord
    {
        public string VideoFilePath { get; private set; }
        public string MetadataFilePath { get; private set; }

        public MovementRecord(string videoFilePath, string metadataFilePath) {
            this.VideoFilePath = videoFilePath;
            this.MetadataFilePath = metadataFilePath;
        }
    }
}
