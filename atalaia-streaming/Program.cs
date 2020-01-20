using atalaia.streaming.objectDetection;
using atalaia.streaming.movementDetection;
using OpenCvSharp;
using OpenCvSharp.Dnn;
using System;
using System.Linq;
using System.Threading;
using System.Reflection;
using System.Collections.Generic;

namespace atalaia.streaming
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Atalaia {0}", Assembly.GetExecutingAssembly().GetName().Version);

            foreach (string url in args)
            {
                Console.WriteLine(url);
                new StreamPipeline(url);
            }
        }
    }
}
