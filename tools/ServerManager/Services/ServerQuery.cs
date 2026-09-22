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

    /// <summary>
    /// The fork's own server query, the packet the game client sends: four 0xFF bytes,
    /// "s2x_getInfo &lt;challenge&gt;", then the S2 trailer (checksum and socket byte). Ported from
    /// tools/server-status.ps1. Without the trailer the server ignores the probe.
    /// </summary>
    internal static class ServerQuery
    {
        private const int SioUdpConnReset = -1744830452;

        /// <summary>Sends one probe per port on one socket and collects what answers in time.</summary>
        public static Dictionary<int, ServerInfo> Query(string host, IList<int> ports, int timeoutMs)
        {
            var results = new Dictionary<int, ServerInfo>();
            if (ports == null || ports.Count == 0) return results;

            using (var udp = new UdpClient(AddressFamily.InterNetwork))
            {
                // A port nobody listens on answers with ICMP "port unreachable", which Windows
                // raises as a ConnectionReset on the next Receive of this shared socket.
                try { udp.Client.IOControl(SioUdpConnReset, new byte[4], null); } catch { }

                var challenges = new Dictionary<int, string>();
                var sent = new Dictionary<int, Stopwatch>();
                foreach (var port in ports)
                {
                    if (challenges.ContainsKey(port)) continue;
                    var challenge = NewChallenge();
                    var packet = Probe(challenge);
                    challenges[port] = challenge;
                    sent[port] = Stopwatch.StartNew();
                    try { udp.Send(packet, packet.Length, host, port); }
                    catch { sent.Remove(port); challenges.Remove(port); }
                }

                var deadline = Stopwatch.StartNew();
                var remote = new IPEndPoint(IPAddress.Any, 0);
                while (results.Count < challenges.Count)
                {
                    var left = timeoutMs - (int)deadline.ElapsedMilliseconds;
                    if (left <= 0) break;
                    udp.Client.ReceiveTimeout = left;

                    byte[] data;
                    try { data = udp.Receive(ref remote); }
                    catch (SocketException ex)
                    {
                        if (ex.SocketErrorCode == SocketError.ConnectionReset) continue;
                        break;
                    }

                    var info = Parse(data);
                    if (info == null) continue;

                    var port = remote.Port;
                    string challenge;
                    if (!challenges.TryGetValue(port, out challenge)) continue;
                    if (info.Get("challenge") != challenge) continue;
                    if (results.ContainsKey(port)) continue;

                    info.Port = port;
                    info.RttMs = (int)sent[port].ElapsedMilliseconds;
                    results[port] = info;
                }
            }
            return results;
        }

        private static byte[] Probe(string challenge)
        {
            var payload = new List<byte> { 0xFF, 0xFF, 0xFF, 0xFF };
            payload.AddRange(Encoding.ASCII.GetBytes("s2x_getInfo " + challenge));
            return WithTrailer(payload.ToArray());
        }

        // Sys_ChecksumCopy: 16-bit ones'-complement sum of big-endian words, carry folded, inverted.
        private static ushort Checksum(byte[] data)
        {
            uint total = 0;
            int i = 0;
            for (; i + 1 < data.Length; i += 2) total += (uint)((data[i] << 8) | data[i + 1]);
            if (data.Length % 2 != 0) total += data[data.Length - 1];
            while ((total >> 16) != 0) total = (total & 0xFFFF) + (total >> 16);
            return (ushort)(~total & 0xFFFF);
        }

        private static byte[] WithTrailer(byte[] payload)
        {
            var sum = Checksum(payload);
            var packet = new byte[payload.Length + 3];
            Buffer.BlockCopy(payload, 0, packet, 0, payload.Length);
            packet[payload.Length] = (byte)(sum >> 8);
            packet[payload.Length + 1] = (byte)(sum & 0xFF);
            // Third byte: (sock NS_CLIENT1 = 0) | (to.localNetID NS_SERVER = 2) << 4
            packet[payload.Length + 2] = 0x20;
            return packet;
        }

        private static ServerInfo Parse(byte[] data)
        {
            if (data == null || data.Length < 8) return null;
            const string head = "s2x_infoResponse\n";
            var text = Encoding.UTF8.GetString(data, 4, data.Length - 4 - 3);
            if (!text.StartsWith(head, StringComparison.Ordinal)) return null;

            var parts = text.Substring(head.Length).Split('\\');
            var info = new ServerInfo();
            for (int i = 1; i + 1 < parts.Length; i += 2) info.Fields[parts[i]] = parts[i + 1];
            return info;
        }

        private static readonly Random Rng = new Random();

        private static string NewChallenge()
        {
            var sb = new StringBuilder(8);
            lock (Rng) for (int i = 0; i < 8; i++) sb.Append("0123456789abcdef"[Rng.Next(16)]);
            return sb.ToString();
        }
    }
}
