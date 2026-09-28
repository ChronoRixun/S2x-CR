using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using S2x.ServerManager.Services;

// Build with the two backend source files. No test framework or external package needed.
internal static class MasterBrowserTests
{
    static void Check(bool condition, string name) { if (!condition) throw new Exception(name); Console.WriteLine("PASS " + name); }
    static byte[] Master(params IPEndPoint[] endpoints)
    {
        var bytes = new List<byte> {255,255,255,255};
        bytes.AddRange(Encoding.ASCII.GetBytes("getserversResponse\n"));
        foreach(var ep in endpoints) { bytes.Add(92); bytes.AddRange(ep.Address.GetAddressBytes()); bytes.Add((byte)(ep.Port>>8));bytes.Add((byte)ep.Port); }
        bytes.AddRange(Encoding.ASCII.GetBytes("\\EOT"));return bytes.ToArray();
    }
    static UdpClient Bound() { var u = new UdpClient(new IPEndPoint(IPAddress.Loopback,0));u.Client.ReceiveTimeout=4000;return u; }
    static string Challenge(byte[] bytes) { var text=Encoding.ASCII.GetString(bytes,4,bytes.Length-7);return text.Substring(text.IndexOf(' ')+1); }
    static byte[] Info(string command,string challenge) { return MasterBrowserClient.Packet(command+"\n\\challenge\\"+challenge+"\\gamename\\S2\\protocol\\1\\hostname\\^2Test server\\mapname\\mp_test\\gametype\\zombies\\mode\\zombies\\clients\\2\\bots\\1\\sv_maxclients\\4\\sv_running\\1"); }
    public static int Main(string[] args)
    {
        try
        {
            if(args.Length>0 && args[0]=="--smoke")
            {
                var r=new MasterBrowserClient().RefreshAsync().GetAwaiter().GetResult();
                Console.WriteLine("Listed="+r.ListedCount+" Error="+r.Error+" Warning="+r.Warning);
                foreach(var s in r.Servers)Console.WriteLine(s.Endpoint+" | "+s.Name+" | "+s.Map+" | "+s.GameType+" | humans="+s.Humans+" bots="+s.Bots+" | "+s.Status);
                return r.Error==null?0:1;
            }
            var ep=new IPEndPoint(IPAddress.Parse("1.2.3.4"),27016);
            List<IPEndPoint> list;bool end;
            Check(MasterBrowserClient.TryMaster(Master(ep),out list,out end)&&end&&list.Count==1&&list[0].Equals(ep),"binary address / big endian port / EOT");
            var eotAddress=new IPEndPoint(IPAddress.Parse("69.79.84.42"),27016);
            Check(MasterBrowserClient.TryMaster(Master(eotAddress),out list,out end)&&end&&list.Count==1&&list[0].Equals(eotAddress),"ASCII EOT IPv4 prefix remains an endpoint");
            Check(!MasterBrowserClient.TryMaster(Master(ep).Take(25).ToArray(),out list,out end),"truncated endpoint rejected");
            var malformed=Master(ep).Concat(new byte[]{17}).ToArray();
            Check(!MasterBrowserClient.TryMaster(malformed,out list,out end),"garbage after EOT rejected");
            Dictionary<string,string> fields;
            Check(MasterBrowserClient.TryInfo(Info("infoResponse","abc"),"infoResponse","abc",out fields),"stock response + trailer");
            Check(!MasterBrowserClient.TryInfo(Info("infoResponse","wrong"),"infoResponse","abc",out fields),"wrong challenge rejected");
            var broken=Info("infoResponse","abc");broken[0]=0;
            Check(!MasterBrowserClient.TryInfo(broken,"infoResponse","abc",out fields),"bad OOB header rejected");
            broken=Info("infoResponse","abc");broken[broken.Length-2]^=1;
            Check(!MasterBrowserClient.TryInfo(broken,"infoResponse","abc",out fields),"bad checksum rejected");
            Check(!MasterBrowserClient.TryInfo(MasterBrowserClient.Packet("infoResponse\n\\challenge\\abc\\challenge\\abc\\gamename\\S2\\protocol\\1"),"infoResponse","abc",out fields),"duplicate keys rejected");
            var counts=new Dictionary<string,string>{{"clients","5"},{"bots","3"},{"s2x_bots","3"}};
            Check(MasterBrowserClient.HumanCount(counts,true)==2,"non-party fork clients include bots");
            counts["party_session"]="1";
            Check(MasterBrowserClient.HumanCount(counts,true)==5,"custom party clients are humans");
            counts.Remove("party_session");
            Check(!MasterBrowserClient.HumanCount(counts,false).HasValue,"legacy fork reply with bots is ambiguous");
            counts["s2x_bots"]="0";
            Check(MasterBrowserClient.HumanCount(counts,false)==5,"legacy fork without bots has known humans");
            counts.Remove("s2x_bots");
            Check(MasterBrowserClient.HumanCount(counts,false)==2,"legacy total minus bots");
            counts["bots"]="7";
            Check(!MasterBrowserClient.HumanCount(counts,true).HasValue,"inconsistent counts remain unknown");
            using(var master=Bound())using(var server=Bound())using(var spoof=Bound())using(var silent=Bound())
            {
                var serverEp=(IPEndPoint)server.Client.LocalEndPoint;
                var silentEp=(IPEndPoint)silent.Client.LocalEndPoint;
                var masterTask=Task.Run(()=>{var remote=new IPEndPoint(IPAddress.Any,0);var req=master.Receive(ref remote);Check(req.Length==19 && Encoding.ASCII.GetString(req,4,15)=="getservers S2 1","master request protocol (no trailer)");var forged=Master(new IPEndPoint(IPAddress.Loopback,1));spoof.Send(forged,forged.Length,remote);
                    var first=Master(serverEp);first=first.Take(first.Length-4).ToArray();master.Send(first,first.Length,remote);
                    var response=Master(serverEp,silentEp);master.Send(response,response.Length,remote);});
                var serverTask=Task.Run(()=>{var remote=new IPEndPoint(IPAddress.Any,0);var req=server.Receive(ref remote);var challenge=Challenge(req);var wrong=Info("s2x_infoResponse",challenge);spoof.Send(wrong,wrong.Length,remote); // correct challenge, wrong source
                    req=server.Receive(ref remote);Check(Encoding.ASCII.GetString(req,4,7)=="getinfo","stock fallback sent");challenge=Challenge(req);wrong=Info("infoResponse","wrong");server.Send(wrong,wrong.Length,remote);var ok=Info("infoResponse",challenge);server.Send(ok,ok.Length,remote);});
                var result=new MasterBrowserClient(1000,250).RefreshAsync(master.Client.LocalEndPoint.ToString()).GetAwaiter().GetResult();
                Task.WaitAll(masterTask,serverTask);
                Check(result.Error==null&&result.ListedCount==2&&result.Servers.Count==2,"dedup master endpoints");
                var good=result.Servers.Single(r=>r.Port==serverEp.Port);var absent=result.Servers.Single(r=>r.Port==silentEp.Port);
                Check(good.Responded&&good.Name=="Test server"&&good.Humans==1&&good.Bots==1&&good.MaxClients==4,"verified fallback fields");
                Check(!absent.Responded&&!absent.Clients.HasValue&&!absent.PingMs.HasValue,"unresponsive stays unknown");
            }
            using(var silent=Bound())using(var cancel=new CancellationTokenSource(80))
            { bool cancelled=false;try{new MasterBrowserClient(3000,100).RefreshAsync(silent.Client.LocalEndPoint.ToString(),cancel.Token).GetAwaiter().GetResult();}catch(OperationCanceledException){cancelled=true;}Check(cancelled,"cancel during UDP wait"); }
            Console.WriteLine("All master browser tests passed.");return 0;
        }
        catch(Exception ex){Console.Error.WriteLine(ex);return 1;}
    }
}
