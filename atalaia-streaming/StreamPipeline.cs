using atalaia.streaming.movementDetection;
using atalaia.streaming.notification;
using atalaia.streaming.objectDetection;
using OpenCvSharp;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;

namespace atalaia.streaming
{
    public class StreamPipeline : IDisposable
    {
        private MovementDetectionProcess movementDetectionProcess;
        private static ObjectDetectionProcess objectDetectionProcess;

        private string videoUrl;
        private VideoCapture cap;
        private volatile uint frames = 0;
        private volatile uint discarded = 0;
        private TimeSpan statsInterval = TimeSpan.FromSeconds(60);
        private Thread captureThread;
        private static uint instances = 0;
        private uint id = instances++;

        public StreamPipeline(string videoUrl)
        {
            movementDetectionProcess = new MovementDetectionProcess(id);

            if (objectDetectionProcess == null)
            {
                objectDetectionProcess = new ObjectDetectionProcess();
                objectDetectionProcess.DetectedObjectEvent += notifyObject;
            }

            this.videoUrl = videoUrl;

            // rtsp://${options.user}:${options.pass}@${options.host}:${options.rtspPort || 554}/cam/realmonitor?channel=${channel}&subtype=1
            cap = new VideoCapture(videoUrl);
            captureThread = new Thread(new ThreadStart(captureLoop));
            captureThread.Name = "Capture";
            captureThread.Priority = ThreadPriority.Highest;
            captureThread.Start();

            movementDetectionProcess.MovementEvent += objectDetectionProcess.Enqueue;
        }

        public void Dispose()
        {
            captureThread.Abort();
        }

        private void captureLoop()
        {
            Timer timer = new Timer(new TimerCallback(printStats), null, statsInterval, statsInterval);

            while (true)
            {
                Mat mat = cap.RetrieveMat();

                frames++;

                if (!mat.Empty())
                {
                    if (!movementDetectionProcess.Enqueue(mat))
                    {
                        this.discarded++;

                        if (this.discarded > 15)
                        {
                            Console.WriteLine($"[{id}] Overload! Reconnecting in 5 seconds...");
                            mat.Dispose();
                            cap.Dispose();
                            Thread.Sleep(TimeSpan.FromSeconds(5));
                            cap = new VideoCapture(videoUrl);
                            Console.WriteLine($"[{id}] Reconnected!");
                        }
                    }
                }
            }
        }

        private void printStats(object state)
        {
            Console.WriteLine($"[{id}] {frames / statsInterval.TotalSeconds} fps; {discarded} discarded; {movementDetectionProcess.MaxAreaDiscarded} maxAreaDiff discarded");
            frames = 0;
            discarded = 0;
            movementDetectionProcess.ResetStats();
        }

        private void notifyObject(MovementRecord record, ICollection<DetectedObject> objects, RecordedFrameMetadata frame, Mat mat)
        {
            foreach (DetectedObject obj in objects)
            {
                mat.DrawMarker(new Point(obj.Left + obj.Width / 2, obj.Top + obj.Height / 2), Scalar.Red, MarkerTypes.Cross, 20, 2);
                mat.PutText($"{obj.Name}: {obj.Confidence}", new Point(obj.Left, obj.Top + obj.Height), HersheyFonts.HersheySimplex, 0.5, Scalar.Red);
            }

            var thumbnail = record.VideoFilePath + ".thumb.jpg";
            Cv2.ImWrite(thumbnail, mat);

            var telegram = new Telegram();
            _ = telegram.SendVideo(record.VideoFilePath, thumbnail);
        }
    }
}
