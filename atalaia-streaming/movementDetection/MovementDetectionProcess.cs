using OpenCvSharp;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Text;
using System.Threading;

namespace atalaia.streaming.movementDetection
{
    public class MovementDetectionProcess : IDisposable
    {
        private uint id;
        private BlockingCollection<FrameData> buffer = new BlockingCollection<FrameData>(30 * 10);
        private Thread thread;
        private List<ProcessedFrame> pastFrames = new List<ProcessedFrame>(30 * 5);
        private BlurDiffMovementDetector movementDetector = new BlurDiffMovementDetector();
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
                var analysis = movementDetector.DetectMovement(frameData.Frame);

                if (analysis.DetectedMovement)
                {
                    pastFrames.Add(new ProcessedFrame() {
                        FrameData = frameData,
                        Movements = analysis.Movements
                    });

                    if (pastFrames.Count > 1 && (frameData.Ts - pastFrames[0].FrameData.Ts).TotalSeconds >= 1.5)
                    {
                        record();
                    } 
                }
                else
                {
                    frameData.Frame.Dispose();
                    pastFrames.ForEach(p => p.FrameData.Frame.Dispose());
                    pastFrames.Clear();
                    MaxAreaDiscarded = Math.Max(MaxAreaDiscarded, analysis.MaxAreaDiff);
                }
            } while (true);
        }

        private void record()
        {
            var begin = pastFrames[0].FrameData.Ts;
            MovementRecord record;

            using (var recorder = new Recorder(id, begin, pastFrames[0].FrameData.Frame.Size()))
            {
                bool shouldRecord = true;
                record = recorder.RecordData;

                foreach (ProcessedFrame pastFrame in pastFrames)
                {
                    recorder.Record(pastFrame.FrameData, pastFrame.Movements);
                    pastFrame.FrameData.Frame.Dispose();
                }

                pastFrames.Clear();

                DateTime? quietSince = null;
                DateTime? restartedAt = null;

                do
                {
                    var frameData = buffer.Take();
                    var analysis = movementDetector.DetectMovement(frameData.Frame);
                    
                    if (!analysis.DetectedMovement)
                    {
                        if (!quietSince.HasValue)
                        {
                            quietSince = frameData.Ts;
                            Console.WriteLine($"[{id}] Quiet...");
                        }
                        else if ((frameData.Ts - quietSince.Value).TotalSeconds >= 5)
                        {
                            shouldRecord = false;
                        }

                        pastFrames.Add(new ProcessedFrame()
                        {
                            FrameData = frameData,
                            Movements = analysis.Movements
                        });
                    }
                    else
                    {
                        if (quietSince.HasValue && !restartedAt.HasValue)
                        {
                            Console.WriteLine($"[{id}] Resume?");
                            restartedAt = DateTime.Now;
                        }
                        else if (quietSince.HasValue && restartedAt.HasValue && (frameData.Ts - restartedAt.Value).TotalSeconds >= 1)
                        {
                            Console.WriteLine($"[{id}] Resume!");

                            foreach (ProcessedFrame pastFrame in pastFrames)
                            {
                                recorder.Record(pastFrame.FrameData, pastFrame.Movements);
                                pastFrame.FrameData.Frame.Dispose();
                            }

                            pastFrames.Clear();
                            restartedAt = null;
                            quietSince = null;
                        }

                        if (!restartedAt.HasValue)
                        {
                            recorder.Record(frameData, analysis.Movements);
                            frameData.Frame.Dispose();

                            if ((frameData.Ts - begin).TotalSeconds >= 30)
                            {
                                shouldRecord = false;
                            }
                        }
                    }
                } while (shouldRecord);

                Console.WriteLine($"[{id}] Stopped.");

                pastFrames.ForEach(p => p.FrameData.Frame.Dispose());
                pastFrames.Clear();
            }

            this.MovementEvent?.Invoke(record);
        }

        public void Dispose()
        {
            this.thread.Abort();
            buffer.Dispose();
            pastFrames.Clear();
        }
    }
}
