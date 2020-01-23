using atalaia.streaming.movementDetection;
using OpenCvSharp;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Linq;
using System.IO;
using System.Text.Json;

namespace atalaia.streaming.objectDetection
{
    public class ObjectDetectionProcess
    {
        private Thread thread;
        private BlockingCollection<MovementRecord> queue = new BlockingCollection<MovementRecord>();
        private YoloV3 objectDetector = new YoloV3();

        public delegate void ObjectDetectionHandler(MovementRecord record, ICollection<DetectedObject> objects, RecordedFrameMetadata frame, Mat mat);

        public event ObjectDetectionHandler DetectedObjectEvent;

        public ObjectDetectionProcess()
        {
            thread = new Thread(process);
            thread.Name = "Object Detection";
            thread.Priority = ThreadPriority.BelowNormal;
            thread.Start();
        }

        public void Enqueue(MovementRecord record)
        {
            queue.Add(record);
        }

        private void process()
        {
            HashSet<string> classesOfInterest = new HashSet<string>();
            classesOfInterest.Add("person");
            //classesOfInterest.Add("car");
            //classesOfInterest.Add("motorbike");
            //classesOfInterest.Add("bicycle");
            //classesOfInterest.Add("bus");
            //classesOfInterest.Add("truck");
            //classesOfInterest.Add("boat");
            classesOfInterest.Add("handbag");
            classesOfInterest.Add("umbrella");
            classesOfInterest.Add("suitcase");
 
            while (true)
            {
                var record = queue.Take();
                bool interested = false;

                Console.WriteLine($"Analysing  {record.VideoFilePath}");

                RecordedFrameMetadata[] frames = (RecordedFrameMetadata[])JsonSerializer.Deserialize(File.ReadAllText(record.MetadataFilePath), typeof(RecordedFrameMetadata[]));
                DateTime? last = null;

                using (var cap = new VideoCapture(record.VideoFilePath))
                {
                    int i = 0;
                    var mat = cap.RetrieveMat();
                    int interval = 0;

                    while (!mat.Empty() && !interested)
                    {
                        if (!last.HasValue || (frames[i].Ts - last.Value).Milliseconds >= interval)
                        {
                            last = frames[i].Ts;
                            var objects = objectDetector.ClassifyImg(mat);

                            foreach (DetectedObject obj in objects)
                            {
                                mat.DrawMarker(new Point(obj.Left + obj.Width / 2, obj.Top + obj.Height / 2), Scalar.Red, MarkerTypes.Cross, 20, 2);
                                mat.PutText($"{obj.Name}: {obj.Confidence}", new Point(obj.Left, obj.Top + obj.Height), HersheyFonts.HersheySimplex, 0.5, Scalar.Red);
                            }

                            if (objects.Any(obj => classesOfInterest.Contains(obj.Name)))
                            {
                                interested = true;
                                Console.WriteLine($"Found {string.Join(", ", objects.Select(obj => obj.Name).ToArray())} objects on {record.VideoFilePath} at {frames[i].Ts}");
                                this.DetectedObjectEvent?.Invoke(record, objects, frames[i], mat);
                            }

                            interval = Math.Min(150, interval + 30);
                        }

                        System.GC.Collect();

                        i++;
                        cap.Read(mat);
                    }

                    mat.Dispose();
                }

                if (!interested)
                {
                    Console.WriteLine($"Discarded {record.VideoFilePath}");
                    File.Delete(record.VideoFilePath);
                    File.Delete(record.MetadataFilePath);
                }
            }
        }
    }
}