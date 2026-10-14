using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Text;

namespace S2x.ServerManager.Services
{
    internal sealed class ServerInfo
    {
        public readonly Dictionary<string, string> Fields = new Dictionary<string, string>(StringComparer.Ordinal);
        public int Port;
        public int RttMs;
        public int Humans;

        public string Get(string key)
        {
            string value;
            return Fields.TryGetValue(key, out value) ? value : null;
        }

        public int Int(string key, int fallback)
        {
            int value;
            return int.TryParse(Get(key), out value) ? value : fallback;
        }
    }

    /// <summary>Challenge-bound raw upstream probes with a validated legacy CR fallback.</summary>
    internal static class ServerQuery
    {
        public static Dictionary<int, ServerInfo> Query(string host, IList<int> ports, int timeoutMs)
        {
            var results = new Dictionary<int, ServerInfo>();
            if (ports == null || ports.Count == 0) return results;
            var addresses = Dns.GetHostAddresses(host);
            var address = Array.Find(addresses, a => a.AddressFamily == AddressFamily.InterNetwork);
            if (address == null) return results;
            using (var udp = new UdpClient(AddressFamily.InterNetwork))
            {
                try { udp.Client.IOControl(-1744830452, new byte[4], null); } catch { }
                for (int attempt = 0; attempt < 4; attempt++)
                {
                    var challenges = new Dictionary<int, string>();
                    var command = attempt < 2 ? "s2x_getInfo" : "getinfo";
                    var watch = Stopwatch.StartNew();
                    foreach (var port in ports)
                    {
                        if (results.ContainsKey(port) || challenges.ContainsKey(port)) continue;
                        var challenge = Guid.NewGuid().ToString("N");
                        var packet = attempt % 2 == 0 ? MasterBrowserClient.RawPacket(command + " " + challenge)
                            : MasterBrowserClient.Packet(command + " " + challenge);
                        try { udp.Send(packet, packet.Length, new IPEndPoint(address, port)); challenges[port] = challenge; }
                        catch (SocketException) { }
                    }
                    var remaining = challenges.Count;
                    while (remaining > 0)
                    {
                        var left = Math.Max(1, timeoutMs / 4) - (int)watch.ElapsedMilliseconds;
                        if (left <= 0) break;
                        udp.Client.ReceiveTimeout = left;
                        var remote = new IPEndPoint(IPAddress.Any, 0);
                        byte[] bytes;
                        try { bytes = udp.Receive(ref remote); }
                        catch (SocketException ex) { if (ex.SocketErrorCode == SocketError.ConnectionReset) continue; break; }
                        string challenge;
                        if (!address.Equals(remote.Address) || results.ContainsKey(remote.Port)
                            || !challenges.TryGetValue(remote.Port, out challenge)) continue;
                        Dictionary<string, string> fields;
                        if (!MasterBrowserClient.TryInfo(bytes, command == "getinfo" ? "infoResponse" : "s2x_infoResponse", challenge, out fields)) continue;
                        var info = new ServerInfo { Port = remote.Port, RttMs = (int)watch.ElapsedMilliseconds };
                        foreach (var pair in fields) info.Fields.Add(pair.Key, pair.Value);
                        // Response framing determines count semantics for older CR party replies.
                        bool legacy = bytes.Length >= 7 && (bytes[bytes.Length - 1] == 0x02 || bytes[bytes.Length - 1] == 0x20)
                            && MasterBrowserClient.Checksum(bytes, bytes.Length - 3) == ((bytes[bytes.Length - 3] << 8) | bytes[bytes.Length - 2]);
                        info.Humans = MasterBrowserClient.HumanCount(fields, command == "s2x_getInfo", legacy) ?? 0;
                        results[remote.Port] = info;
                        remaining--;
                    }
                    if (results.Count == ports.Count) break;
                }
            }
            return results;
        }
    }
}
