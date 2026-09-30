using System;
using System.IO;
using System.IO.Pipes;
using System.Text;
using System.Web.Script.Serialization;

// A local synthetic pipe, not a game server. Tests never connect to a live S2x process.
public static class AdminPipeFixture
{
    public static void Main(string[] args)
    {
        var scenario = args[0]; var nonce = args[1];
        if (scenario == "idle") { System.Threading.Thread.Sleep(15000); return; }
        using (var pipe = new NamedPipeServerStream("S2x.ServerAdmin." + nonce, PipeDirection.InOut, 1))
        {
            pipe.WaitForConnection();
            if (scenario == "silent") { System.Threading.Thread.Sleep(15000); return; }
            var size = BitConverter.ToInt32(Read(pipe, 4), 0);
            if (size < 1 || size > 8192) return;
            var serializer = new JavaScriptSerializer();
            var request = serializer.Deserialize<System.Collections.Generic.Dictionary<string, object>>(Encoding.UTF8.GetString(Read(pipe, size)));
            var reply = new { version = 1, id = scenario == "wrong-id" ? "wrong" : (string)request["id"], ok = true, code = "accepted", message = "Synthetic response", instance = nonce,
                pid = System.Diagnostics.Process.GetCurrentProcess().Id, noticeReady = true, recipients = 1,
                players = new[] { new { slot = 3, name = "Fixture human", token = "fake-token", isBot = false, isHost = false, state = "active" } } };
            var bytes = Encoding.UTF8.GetBytes(serializer.Serialize(reply));
            var length = BitConverter.GetBytes(scenario == "oversize" ? 8193 : bytes.Length);
            pipe.Write(length, 0, 4);
            if (scenario != "oversize") pipe.Write(bytes, 0, bytes.Length);
            pipe.Flush();
            System.Threading.Thread.Sleep(500);
        }
    }
    private static byte[] Read(Stream stream, int count)
    {
        var data = new byte[count]; var offset = 0;
        while (offset < count) { var read = stream.Read(data, offset, count - offset); if (read == 0) throw new EndOfStreamException(); offset += read; }
        return data;
    }
}
