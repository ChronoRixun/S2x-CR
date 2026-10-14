using System;

namespace S2x.ServerManager.Models
{
    internal enum ServerStatus
    {
        Stopped,        // no process, no pid file
        Starting,       // process alive, no answer yet, within the first 90 s
        Running,        // answers queries
        NotAnswering,   // process alive, three consecutive query misses
        Crashed,        // pid file present, process gone, and we did not stop it
    }

    /// <summary>What one poll round found for one port.</summary>
    internal sealed class ServerState
    {
        public const int StartupGraceSeconds = 90;
        public const int MissesBeforeStale = 3;

        public int Port;
        public ServerStatus Status = ServerStatus.Stopped;
        public int Pid;
        public DateTime? ProcessStart;   // local time, for uptime
        public int Misses;

        public DateTime? LastReply;      // local time of the last reply
        public DateTime? NoticedGone;    // when this app first saw the process missing

        public int Humans;
        public int Bots;
        public int Cap;
        public int PingMs;
        public string MapKey;
        public string GametypeKey;
        public bool SvRunning;

        public bool IsLive { get { return Status == ServerStatus.Running || Status == ServerStatus.Starting; } }
        public bool NeedsAttention { get { return Status == ServerStatus.NotAnswering || Status == ServerStatus.Crashed; } }
        public int Free { get { return Math.Max(0, Cap - Humans - Bots); } }

        public TimeSpan? Uptime
        {
            get { return ProcessStart.HasValue ? (TimeSpan?)(DateTime.Now - ProcessStart.Value) : null; }
        }

        public static string FormatSpan(TimeSpan? span)
        {
            if (!span.HasValue || span.Value.TotalSeconds < 0) return "—";
            var t = span.Value;
            if (t.TotalDays >= 1) return string.Format("{0}d {1}h", (int)t.TotalDays, t.Hours);
            if (t.TotalHours >= 1) return string.Format("{0}h {1:00}m", (int)t.TotalHours, t.Minutes);
            if (t.TotalMinutes >= 1) return string.Format("{0}m", (int)t.TotalMinutes);
            return string.Format("{0} s", (int)t.TotalSeconds);
        }
    }
}
