using OpenCvSharp;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Linq;

namespace atalaia.streaming.movementDetection
{
    public class MovementDetectionProcess : IDisposable
    {
        private uint id;
        private BlockingCollection<FrameData> buffer = new BlockingCollection<FrameData>(30 * 10);
        private Thread thread;
        private BufferedBlurDiffMovementDetector movementDetector = new BufferedBlurDiffMovementDetector();
        public double MaxAreaDiscarded { get; private set; }

        public delegate void DetectedMovementHandler(MovementRecord record);

        public event DetectedMovementHandler MovementEvent;

        public MovementDetectionProcess(uint id)
        {
            this.id = id;
            thread = new Thread(new ThreadStart(process));
            thread.Name = "Movement Detection " + id;
            thread.Start();
        }

        public bool Enqueue(Mat mat)
        {
            return buffer.TryAdd(new FrameData(mat));
        }

        public void ResetStats()
        {
            this.MaxAreaDiscarded = 0;
        }

        private void process()
        {
            do
            {
                var frameData = buffer.Take();
                var result = movementDetector.DetectMovement(frameData);

                if (result.Length > 0)
                {
                    record(result);
                }
            } while (true);
        }

        private void record(DetectedMovementWithFrame[] pastFrames)
        {
            var begin = pastFrames[0].FrameData.Ts;
            MovementRecord record;

            using (var recorder = new Recorder(id, begin, pastFrames[0].FrameData.Frame.Size()))
            {
                bool shouldRecord = true;
                record = recorder.RecordData;

                foreach (DetectedMovementWithFrame pastFrame in pastFrames)
                {
                    recorder.Record(pastFrame.FrameData, pastFrame.Analysis.Movements);
                    pastFrame.FrameData.Frame.Dispose();
                }

                DateTime? quietSince = null;

                do
                {
                    var frameData = buffer.Take();
                    var analysis = movementDetector.DetectMovement(frameData);
                    
                    if (analysis.Length == 0)
                    {
                        if (!quietSince.HasValue)
                        {
                            quietSince = frameData.Ts;
                            Console.WriteLine($"[{id}] {DateTime.Now} -- Pause...");
                        }
                        else if ((frameData.Ts - quietSince.Value).TotalSeconds >= 5)
                        {
                            shouldRecord = false;
                        }
                    }
                    else
                    {
                        Console.WriteLine($"[{id}] {DateTime.Now} --  Resume!");

                        foreach (DetectedMovementWithFrame pastFrame in analysis)
                        {
                            recorder.Record(pastFrame.FrameData, pastFrame.Analysis.Movements);
                            pastFrame.FrameData.Frame.Dispose();
                        }

                        quietSince = null;
                    }

                    if ((frameData.Ts - begin).TotalSeconds >= 30)
                        shouldRecord = false;

                } while (shouldRecord);

                Console.WriteLine($"[{id}] {DateTime.Now} --  Stopped.");
            }

            this.MovementEvent?.Invoke(record);
        }

        public void Dispose()
        {
            this.thread.Abort();
            buffer.Dispose();
        }
    }
}
