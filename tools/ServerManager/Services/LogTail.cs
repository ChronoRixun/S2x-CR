using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace S2x.ServerManager.Services
{
    /// <summary>What one read of the log found.</summary>
    internal sealed class LogChunk
    {
        public readonly List<string> Lines = new List<string>();

        /// <summary>The file is smaller than it was: the server started again over the top of it.</summary>
        public bool Cleared;

        /// <summary>The file is not there. Not an error: it appears when the server first writes.</summary>
        public bool Missing;
    }

    /// <summary>
    /// Reads the new tail of a log file the game is still writing to. Opened
    /// FileShare.ReadWrite | Delete so the server keeps writing and can be shut down and
    /// replaced underneath us, and never opened for writing: nothing here deletes or truncates
    /// a log. Only whole lines come out, so a line caught halfway through a write waits for
    /// the round that finishes it.
    /// </summary>
    internal sealed class LogTail
    {
        /// <summary>How far back the first read goes. A shared console.log runs to megabytes.</summary>
        private const int FirstReadBytes = 192 * 1024;

        /// <summary>The most one round takes, so a server writing hard cannot stall the poll.</summary>
        private const int ChunkBytes = 512 * 1024;

        private long _offset;
        private bool _opened;

        public LogTail(string path) { Path = path; }

        public string Path { get; private set; }

        public LogChunk Read()
        {
            var chunk = new LogChunk();
            try
            {
                if (!File.Exists(Path))
                {
                    // Gone, or not written yet. Either way the next file to appear is a new one.
                    _opened = false;
                    _offset = 0;
                    chunk.Missing = true;
                    return chunk;
                }

                using (var stream = new FileStream(Path, FileMode.Open, FileAccess.Read,
                                                   FileShare.ReadWrite | FileShare.Delete))
                {
                    var length = stream.Length;
                    var partialFirstLine = false;

                    if (!_opened)
                    {
                        // Open on the tail rather than on fourteen megabytes of history.
                        _opened = true;
                        _offset = Math.Max(0, length - FirstReadBytes);
                        partialFirstLine = _offset > 0;
                    }
                    else if (length < _offset)
                    {
                        _offset = 0;
                        chunk.Cleared = true;
                    }

                    if (length == _offset) return chunk;

                    var want = (int)Math.Min(length - _offset, ChunkBytes);
                    var buffer = new byte[want];
                    stream.Seek(_offset, SeekOrigin.Begin);
                    var got = 0;
                    while (got < want)
                    {
                        var read = stream.Read(buffer, got, want - got);
                        if (read <= 0) break;
                        got += read;
                    }
                    if (got == 0) return chunk;

                    // Up to the last line break: what comes after it is a line still being
                    // written, and half a line is not a line.
                    var end = LastBreak(buffer, got);
                    if (end < 0)
                    {
                        // A chunk this big with no break in it is not a line either, but holding
                        // it back would stall the tail for good, so take it as one.
                        if (got < ChunkBytes) return chunk;
                        end = got;
                    }

                    _offset += end;
                    var text = Encoding.UTF8.GetString(buffer, 0, end);
                    foreach (var line in text.Split('\n'))
                        chunk.Lines.Add(line.TrimEnd('\r'));
                    // Split leaves an empty tail after the final break.
                    if (chunk.Lines.Count > 0 && chunk.Lines[chunk.Lines.Count - 1].Length == 0)
                        chunk.Lines.RemoveAt(chunk.Lines.Count - 1);
                    if (partialFirstLine && chunk.Lines.Count > 0) chunk.Lines.RemoveAt(0);
                    return chunk;
                }
            }
            catch (IOException)
            {
                // Locked for a moment by the writer. Try again next round.
                return chunk;
            }
            catch (UnauthorizedAccessException)
            {
                return chunk;
            }
        }

        private static int LastBreak(byte[] buffer, int count)
        {
            for (int i = count - 1; i >= 0; i--)
                if (buffer[i] == (byte)'\n') return i + 1;
            return -1;
        }
    }
}
