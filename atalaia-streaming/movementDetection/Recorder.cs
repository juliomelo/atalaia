using OpenCvSharp;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json;

namespace atalaia.streaming.movementDetection
{
    class Recorder : IDisposable
    {
        private static ulong seq = 0;
        private uint id;
        private VideoWriter writer;
        private StreamWriter metadataWriter;
        private ulong frames = 0;
        public MovementRecord RecordData { get; private set; }

        public Recorder(uint id, DateTime ts, Size size)
        {
            this.id = id;
            RecordData = new MovementRecord($"/data/local/{seq}-{id}.mp4", $"/data/local/{seq++}-{id}.json");
            writer = new VideoWriter(RecordData.VideoFilePath, FourCC.FromEnum(FourCCValues.H264), 30, size);
            metadataWriter = new StreamWriter(RecordData.MetadataFilePath);

            Console.WriteLine($"[{id}] {DateTime.Now} -- Recording {RecordData.VideoFilePath}");
        }

        public void Record(FrameData frameData, IEnumerable<DetectedMovement> movements)
        {
            writer.Write(frameData.Frame);

            var json = JsonSerializer.Serialize(new RecordedFrameMetadata()
            {
                Ts = frameData.Ts,
                Movements = movements
            });

            if (frames++ == 0)
            {
                metadataWriter.Write('[');
            }
            else
            {
                metadataWriter.Write(',');
            }

            metadataWriter.Write(json);
        }

        public void Dispose()
        {
            metadataWriter.Write(']');
            writer.Release();
            metadataWriter.Close();
            writer.Dispose();
            metadataWriter.Dispose();
        }
    }
}
