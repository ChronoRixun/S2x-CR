using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using S2x.ServerManager.Models;

namespace S2x.ServerManager.Services
{
    /// <summary>Read-only S2 protocol 1 master discovery and challenge-bound UDP info queries.</summary>
    internal sealed class MasterBrowserClient
    {
        public const string DefaultHost = "master.s2x.dev:20810";
        private const int Limit = 128;
        private readonly int masterTimeout;
        private readonly int queryTimeout;
        public MasterBrowserClient() : this(3500, 1200) { }
        internal MasterBrowserClient(int masterTimeoutMs, int queryTimeoutMs)
        { masterTimeout = masterTimeoutMs; queryTimeout = queryTimeoutMs; }

        public async Task<MasterBrowserResult> RefreshAsync(string host = DefaultHost,
            CancellationToken cancellationToken = default(CancellationToken))
        {
            var result = new MasterBrowserResult();
            try
            {
                cancellationToken.ThrowIfCancellationRequested();
                if (string.IsNullOrWhiteSpace(host)) host = DefaultHost;
                var bits = host.Split(':');
                int port = 20810;
                if (bits.Length > 2 || bits[0].Length == 0 || (bits.Length == 2 &&
                    (!int.TryParse(bits[1], out port) || port < 1 || port > 65535)))
                    throw new ArgumentException("Use a master hostname or IPv4 address, optionally followed by :port.");
                var lookup = Dns.GetHostAddressesAsync(bits[0]);
                if (await Task.WhenAny(lookup, Task.Delay(masterTimeout, cancellationToken)).ConfigureAwait(false) != lookup)
                { cancellationToken.ThrowIfCancellationRequested(); throw new TimeoutException("Master DNS lookup timed out."); }
                var address = (await lookup.ConfigureAwait(false)).FirstOrDefault(a => a.AddressFamily == AddressFamily.InterNetwork);
                if (address == null) throw new ArgumentException("The master has no IPv4 address.");
                var master = new IPEndPoint(address, port);
                var discovery = await Task.Run(() => Discover(master, cancellationToken), cancellationToken).ConfigureAwait(false);
                result.ListedCount = discovery.Endpoints.Count;
                if (!discovery.Received) result.Error = "No valid master response received; server availability is unknown.";
                else if (!discovery.Complete) result.Warning = "Master response was incomplete; showing the endpoints received.";
                if (discovery.Endpoints.Count == Limit) result.Warning = "The browser is limited to 128 endpoints; the list may be truncated.";
                using (var slots = new SemaphoreSlim(8))
                {
                    var work = discovery.Endpoints.Select(async endpoint =>
                    {
                        await slots.WaitAsync(cancellationToken).ConfigureAwait(false);
                        try { return await Task.Run(() => Query(endpoint, cancellationToken), cancellationToken).ConfigureAwait(false); }
                        finally { slots.Release(); }
                    }).ToArray();
                    var rows = await Task.WhenAll(work).ConfigureAwait(false);
                    result.Servers = rows.OrderByDescending(r => r.Responded).ThenByDescending(r => r.Humans)
                        .ThenBy(r => r.Name, StringComparer.OrdinalIgnoreCase).ThenBy(r => r.Endpoint).ToArray();
                }
            }
            catch (OperationCanceledException) { throw; }
            catch (Exception ex) when (ex is SocketException || ex is ArgumentException || ex is TimeoutException)
            { result.Error = ex.Message; }
            result.CompletedUtc = DateTime.UtcNow;
            return result;
        }

        private sealed class Discovery
        {
            public readonly List<IPEndPoint> Endpoints = new List<IPEndPoint>();
            public bool Received;
            public bool Complete;
        }

        private Discovery Discover(IPEndPoint master, CancellationToken token)
        {
            var result = new Discovery();
            using (var udp = Socket())
            {
                // The master speaks standard OOB (no S2 trailer); game-server queries require it.
                var masterPacket = Packet("getservers S2 1");
                Send(udp, masterPacket.Take(masterPacket.Length - 3).ToArray(), master);
                var watch = Stopwatch.StartNew();
                int packets = 0;
                while (watch.ElapsedMilliseconds < masterTimeout && packets < 256)
                {
                    token.ThrowIfCancellationRequested();
                    IPEndPoint remote;
                    var bytes = Receive(udp, out remote);
                    if (bytes == null) continue;
                    packets++;
                    if (!master.Equals(remote)) continue;
                    List<IPEndPoint> endpoints;
                    bool end;
                    if (!TryMaster(bytes, out endpoints, out end)) continue;
                    result.Received = true;
                    foreach (var endpoint in endpoints)
                        if (result.Endpoints.Count < Limit && !result.Endpoints.Contains(endpoint)) result.Endpoints.Add(endpoint);
                    if (end) { result.Complete = true; break; }
                }
            }
            return result;
        }

        private MasterServerRow Query(IPEndPoint endpoint, CancellationToken token)
        {
            var row = new MasterServerRow { Address = endpoint.Address.ToString(), Port = endpoint.Port,
                Name = endpoint.ToString(), Status = "No response (availability unknown)" };
            try
            {
                using (var udp = Socket())
                {
                    // Brent's stock S2x understands getinfo; the fork also offers richer s2x_getInfo.
                    foreach (var command in new[] { "s2x_getInfo", "getinfo" })
                    {
                        token.ThrowIfCancellationRequested();
                        var challenge = Guid.NewGuid().ToString("N");
                        Send(udp, Packet(command + " " + challenge), endpoint);
                        var watch = Stopwatch.StartNew();
                        int packets = 0;
                        while (watch.ElapsedMilliseconds < queryTimeout && packets < 128)
                        {
                            token.ThrowIfCancellationRequested();
                            IPEndPoint remote;
                            var bytes = Receive(udp, out remote);
                            if (bytes == null) continue;
                            packets++;
                            if (!endpoint.Equals(remote)) continue;
                            Dictionary<string, string> fields;
                            if (!TryInfo(bytes, command == "getinfo" ? "infoResponse" : "s2x_infoResponse", challenge, out fields)) continue;
                            row.Responded = true;
                            row.Name = Clean(Get(fields, "hostname") ?? Get(fields, "sv_hostname") ?? endpoint.ToString());
                            row.Map = Clean(Get(fields, "mapname"));
                            row.GameType = Clean(Get(fields, "gametype"));
                            row.Mode = Clean(Get(fields, "mode"));
                            row.Clients = Number(fields, "clients");
                            row.Bots = Number(fields, "s2x_bots") ?? Number(fields, "bots");
                            row.MaxClients = Number(fields, "sv_maxclients") ?? Number(fields, "maxclients");
                            row.Humans = HumanCount(fields, command == "s2x_getInfo");
                            row.PingMs = (int)watch.ElapsedMilliseconds;
                            row.Status = Get(fields, "sv_running") == "0" ? "Responding (lobby)" : "Responding";
                            return row;
                        }
                    }
                }
            }
            catch (SocketException) { row.Status = "Query failed (availability unknown)"; }
            return row;
        }

        private static UdpClient Socket()
        {
            var udp = new UdpClient(AddressFamily.InterNetwork);
            udp.Client.ReceiveTimeout = 100;
            udp.Client.SendTimeout = 500;
            try { udp.Client.IOControl(-1744830452, new byte[4], null); } catch (SocketException) { }
            return udp;
        }
        private static void Send(UdpClient udp, byte[] packet, IPEndPoint endpoint) { udp.Send(packet, packet.Length, endpoint); }
        private static byte[] Receive(UdpClient udp, out IPEndPoint remote)
        {
            remote = new IPEndPoint(IPAddress.Any, 0);
            try { return udp.Receive(ref remote); }
            catch (SocketException ex) when (ex.SocketErrorCode == SocketError.TimedOut || ex.SocketErrorCode == SocketError.WouldBlock || ex.SocketErrorCode == SocketError.ConnectionReset) { return null; }
        }
        internal static ushort Checksum(byte[] bytes, int length)
        {
            uint sum = 0; int i;
            for (i = 0; i + 1 < length; i += 2) sum += (uint)((bytes[i] << 8) | bytes[i + 1]);
            if (i < length) sum += bytes[i];
            while ((sum >> 16) != 0) sum = (sum & 65535) + (sum >> 16);
            return (ushort)~sum;
        }
        internal static byte[] Packet(string text)
        {
            var body = Encoding.UTF8.GetBytes(text);
            var result = new byte[body.Length + 7];
            for (int i = 0; i < 4; i++) result[i] = 255;
            Buffer.BlockCopy(body, 0, result, 4, body.Length);
            var sum = Checksum(result, result.Length - 3);
            result[result.Length - 3] = (byte)(sum >> 8);
            result[result.Length - 2] = (byte)sum;
            result[result.Length - 1] = 0x20;
            return result;
        }
        private static int PayloadLength(byte[] bytes)
        {
            if (bytes == null || bytes.Length < 5 || bytes.Length > 16384) return 0;
            for (int i = 0; i < 4; i++) if (bytes[i] != 255) return 0;
            if (bytes.Length >= 7 && Checksum(bytes, bytes.Length - 3) == ((bytes[bytes.Length - 3] << 8) | bytes[bytes.Length - 2])) return bytes.Length - 3;
            return bytes.Length; // Masters use standard untrailed OOB packets.
        }
        internal static bool TryMaster(byte[] bytes, out List<IPEndPoint> endpoints, out bool end)
        {
            endpoints = new List<IPEndPoint>(); end = false;
            int length = PayloadLength(bytes);
            const string header = "getserversResponse";
            if (length < 4 + header.Length || Encoding.ASCII.GetString(bytes, 4, header.Length) != header) return false;
            int at = 4 + header.Length;
            if (at < length && (bytes[at] == 10 || bytes[at] == 32)) at++;
            while (at < length)
            {
                if (bytes[at] != 92) return false;
                if (at + 4 <= length && bytes[at + 1] == 'E' && bytes[at + 2] == 'O' && bytes[at + 3] == 'T')
                {
                    // 69.79.84.x starts with ASCII EOT too. Treat it as a terminator
                    // only if the entire suffix is permitted padding; otherwise read an endpoint.
                    int suffix = at + 4;
                    while (suffix < length && (bytes[suffix] == 0 || bytes[suffix] == 92)) suffix++;
                    if (suffix == length) { end = true; return true; }
                }
                if (at + 7 > length) return false;
                var address = new IPAddress(new[] { bytes[at + 1], bytes[at + 2], bytes[at + 3], bytes[at + 4] });
                int port = (bytes[at + 5] << 8) | bytes[at + 6];
                if (port != 0 && !address.Equals(IPAddress.Any) && !address.Equals(IPAddress.Broadcast) && bytes[at + 1] < 224)
                    endpoints.Add(new IPEndPoint(address, port));
                at += 7;
            }
            return true;
        }
        internal static bool TryInfo(byte[] bytes, string command, string challenge, out Dictionary<string, string> fields)
        {
            fields = new Dictionary<string, string>(StringComparer.Ordinal);
            int length = PayloadLength(bytes);
            // Both supported S2x query commands reply through NET_SendPacket with this trailer.
            if (length < 5 || length > 8192 || length != bytes.Length - 3) return false;
            string text;
            try { text = new UTF8Encoding(false, true).GetString(bytes, 4, length - 4); }
            catch (DecoderFallbackException) { return false; }
            if (!text.StartsWith(command + "\n", StringComparison.Ordinal)) return false;
            text = text.Substring(command.Length + 1).TrimEnd('\0');
            if (!text.StartsWith("\\", StringComparison.Ordinal) || text.Any(c => char.IsControl(c))) return false;
            var parts = text.Split('\\');
            if (parts.Length % 2 != 1 || parts.Length > 257) return false;
            for (int i = 1; i < parts.Length; i += 2)
            {
                if (parts[i].Length == 0 || fields.ContainsKey(parts[i])) return false;
                fields.Add(parts[i], parts[i + 1]);
            }
            return Get(fields, "challenge") == challenge && Get(fields, "gamename") == "S2" && Get(fields, "protocol") == "1";
        }
        internal static int? HumanCount(Dictionary<string, string> fields, bool customResponse)
        {
            var clients = Number(fields, "clients");
            var bots = Number(fields, "s2x_bots") ?? Number(fields, "bots");
            if (!clients.HasValue) return null;
            if (customResponse)
            {
                if (Get(fields, "party_session") == "1") return clients;
                // Unknown session semantics must not become an invented human count.
                if (fields.ContainsKey("party_session")) return null;
            }
            else if (fields.ContainsKey("s2x_bots"))
            {
                // getinfo omits party_session even when clients means party humans.
                // With bots present, total-client and party-human interpretations differ.
                return bots == 0 ? clients : (int?)null;
            }
            return bots.HasValue && bots.Value <= clients.Value ? clients.Value - bots.Value : (int?)null;
        }
        private static string Get(Dictionary<string, string> fields, string key) { string value; return fields.TryGetValue(key, out value) ? value : null; }
        private static int? Number(Dictionary<string, string> fields, string key)
        { int value; return int.TryParse(Get(fields, key), NumberStyles.None, CultureInfo.InvariantCulture, out value) && value >= 0 && value <= 128 ? value : (int?)null; }
        private static string Clean(string value)
        {
            if (value == null) return null;
            var sb = new StringBuilder();
            for (int i = 0; i < value.Length && sb.Length < 160; i++)
            { if (value[i] == '^' && i + 1 < value.Length && char.IsDigit(value[i + 1])) { i++; continue; } if (!char.IsControl(value[i])) sb.Append(value[i]); }
            return sb.ToString();
        }
    }
}
