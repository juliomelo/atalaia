using OpenCvSharp;
using System;
using System.Collections.Generic;
using System.Text;

namespace atalaia.streaming
{
    public static class MatOperation
    {
        public delegate void ExecuteHandler(Mat source, Mat output);

        public static Mat ExecuteOperations(this Mat source, bool clone, params ExecuteHandler[] handlers)
        {
            Mat[] mat = new Mat[2];
            mat[0] = clone ? source.Clone() : source;
            mat[1] = source.EmptyClone();
            int idx = 0;

            foreach (ExecuteHandler handler in handlers)
            {
                int outputIdx = idx ^ 1;
                handler(mat[idx], mat[outputIdx]);
                idx = outputIdx;
            }

            mat[idx ^ 1].Dispose();

            return mat[idx];
        }
    }
}
