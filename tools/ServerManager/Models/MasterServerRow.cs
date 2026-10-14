using System;
using System.Collections.Generic;

namespace S2x.ServerManager.Models
{
    internal sealed class MasterServerRow
    {
        public string Address { get; set; }
        public int Port { get; set; }
        public string Endpoint { get { return Address + ":" + Port; } }
        public string Name { get; set; }
        public string Map { get; set; }
        public string GameType { get; set; }
        public string Mode { get; set; }
        public int? Clients { get; set; }
        public int? Humans { get; set; }
        public int? Bots { get; set; }
        public int? MaxClients { get; set; }
        public int? PingMs { get; set; }
        public bool Responded { get; set; }
        public string Status { get; set; }
    }

    internal sealed class MasterBrowserResult
    {
        public IReadOnlyList<MasterServerRow> Servers { get; set; } = new MasterServerRow[0];
        public int ListedCount { get; set; }
        public string Error { get; set; }
        public string Warning { get; set; }
        public DateTime CompletedUtc { get; set; }
    }
}
