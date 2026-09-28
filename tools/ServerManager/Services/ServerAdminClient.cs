using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using Microsoft.Win32.SafeHandles;

namespace S2x.ServerManager.Services
{
    public sealed class AdminPlayer
    {
        public int slot { get; set; }
        public string name { get; set; }
        public string token { get; set; }
        public bool isBot { get; set; }
        public bool isHost { get; set; }
        public string state { get; set; }
        public string Kind => isBot ? "Bot" : isHost ? "Host" : "Human";
    }
    public sealed class AdminReply
    {
        public int version { get; set; }
        public string id { get; set; }
        public bool ok { get; set; }
        public string code { get; set; }
        public string message { get; set; }
        public string instance { get; set; }
        public int pid { get; set; }
        public bool noticeReady { get; set; }
        public List<AdminPlayer> players { get; set; }
        public int recipients { get; set; }
        public string AuditWarning { get; set; }
    }
    public sealed class ServerAdminClient
    {
        private readonly ManagedServerOwnershipStore _store;
        public ServerAdminClient(ManagedServerOwnershipStore store = null) { _store = store ?? new ManagedServerOwnershipStore(); }
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetNamedPipeServerProcessId(SafePipeHandle pipe, out uint serverProcessId);
        public Task<AdminReply> RequestAsync(int port, string operation, string target = null, string message = null)
        {
            return Task.Run(() => Request(port, operation, target, message));
        }
        private AdminReply Request(int port, string operation, string target, string message)
        {
            if (operation != "hello" && operation != "players" && operation != "announce" && operation != "warn" && operation != "kick") throw new ArgumentException("Unsupported administration operation.");
            var id = Guid.NewGuid().ToString("D");
            AdminReply reply;
            var sent = false;
            try
            {
                if (message != null && (Encoding.UTF8.GetByteCount(message) > 160 || System.Linq.Enumerable.Any(message, char.IsControl))) throw new ArgumentException("Use a single-line message of at most 160 UTF-8 bytes.");
                var record = _store.Read(port);
                _store.Validate(record);
                using (var pipe = new NamedPipeClientStream(".", "S2x.ServerAdmin." + record.Instance, PipeDirection.InOut, PipeOptions.Asynchronous))
                using (var timeout = new Timer(s => { try { pipe.Dispose(); } catch { } }, null, 5000, Timeout.Infinite))
                {
                    pipe.Connect(2000);
                    uint pipePid;
                    if (!GetNamedPipeServerProcessId(pipe.SafePipeHandle, out pipePid) || pipePid != record.Pid) throw new InvalidOperationException("Administration pipe belongs to another process. No request was sent.");
                    _store.Validate(record);
                    var request = new { version = 1, id, operation, instance = record.Instance, target, message };
                    var json = new JavaScriptSerializer();
                    var payload = Encoding.UTF8.GetBytes(json.Serialize(request));
                    if (payload.Length > 8192) throw new InvalidDataException("Administration request is too large.");
                    var size = BitConverter.GetBytes(payload.Length);
                    sent = true; // From this point a disconnect cannot prove the action did not run.
                    pipe.Write(size, 0, size.Length); pipe.Write(payload, 0, payload.Length); pipe.Flush();
                    var length = BitConverter.ToInt32(ReadExact(pipe, 4), 0);
                    if (length <= 0 || length > 8192) throw new InvalidDataException("Invalid administration reply size.");
                    reply = json.Deserialize<AdminReply>(new UTF8Encoding(false, true).GetString(ReadExact(pipe, length)));
                    if (reply == null || reply.version != 1 || reply.id != id || reply.instance != record.Instance || reply.pid != record.Pid)
                        throw new InvalidDataException("Administration reply did not match this request and server instance.");
                }
            }
            catch (Exception ex)
            {
                reply = new AdminReply { id = id, ok = false, code = "unavailable_or_unconfirmed", message = ex.Message + (sent ? " The request outcome is unconfirmed; no automatic retry was made." : " No action was sent.") };
            }
            if (operation != "hello" && operation != "players")
            {
                try
                {
                    Directory.CreateDirectory(_store.DirectoryPath);
                    var entry = new { utc = DateTime.UtcNow.ToString("o"), id, port, operation, target, message, reply.ok, reply.code, outcome = reply.message, reply.instance, reply.pid };
                    lock (AuditLock) File.AppendAllText(Path.Combine(_store.DirectoryPath, "actions.jsonl"), new JavaScriptSerializer().Serialize(entry) + Environment.NewLine, new UTF8Encoding(false));
                }
                catch (Exception ex) { reply.AuditWarning = "Local audit could not be saved: " + ex.Message; }
            }
            return reply;
        }
        private static readonly object AuditLock = new object();
        private static byte[] ReadExact(Stream stream, int size)
        {
            var bytes = new byte[size]; var offset = 0;
            while (offset < size) { var read = stream.Read(bytes, offset, size - offset); if (read == 0) throw new EndOfStreamException("Server closed the administration pipe."); offset += read; }
            return bytes;
        }
    }
}
