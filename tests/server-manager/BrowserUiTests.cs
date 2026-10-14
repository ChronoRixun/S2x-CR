using System;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using S2x.ServerManager.Models;
using S2x.ServerManager.ViewModels;

class BrowserUiTests
{
 static void Check(bool ok,string message) { if(!ok)throw new Exception(message); Console.WriteLine("PASS "+message); }
 static int Count(MasterBrowserViewModel vm) { return vm.FilteredRows.Cast<object>().Count(); }
 [STAThread] static int Main()
 {
  try {
   var when=new DateTime(2026,9,27,12,0,0,DateTimeKind.Utc);
   var data=new MasterBrowserResult {ListedCount=3,CompletedUtc=when,Servers=new[]{
    new MasterServerRow {Address="192.0.2.1",Port=27040,Name="^1CR's ^7Olympus",Map="mp_zombie_dnk_srv",GameType="zombies",Clients=1,Humans=1,Bots=3,MaxClients=4,PingMs=20,Responded=true},
    new MasterServerRow {Address="192.0.2.2",Port=27016,Name="MP rotation",Map="mp_london",GameType="war",Clients=12,Humans=2,Bots=10,MaxClients=18,PingMs=30,Responded=true},
    new MasterServerRow {Address="192.0.2.3",Port=27017}
   }};
   MasterBrowserResult current=data; var calls=0;
   Func<string,CancellationToken,Task<MasterBrowserResult>> work=(h,t)=>{calls++;return Task.FromResult(current);};
   var vm=new MasterBrowserViewModel((h,t)=>work(h,t));
   vm.RefreshAsync().GetAwaiter().GetResult();
   Check(vm.Rows.Count==3&&Count(vm)==3,"successful refresh shows every listed server");
   Check(vm.Rows.Single(r=>r.Source.GameType=="zombies").Map=="U.S.S. Mount Olympus","survival map has friendly name");
   Check(vm.Rows.Single(r=>r.Source.GameType=="zombies").Humans=="1","human count is not confused with human-plus-bot occupancy");
   Check(vm.Rows.Single(r=>!r.Source.Responded).Humans=="?"&&vm.Rows.Single(r=>!r.Source.Responded).Bots=="?","no reply preserves unknown counts");
   vm.Selected=vm.Rows.Single(r=>r.Source.GameType=="zombies");
   vm.Filter="Multiplayer";Check(vm.Selected==null&&!vm.CopyCommand.CanExecute(null),"filtering out selection disables copy");
   vm.Filter="Zombies";Check(Count(vm)==1,"Zombies filter");
   vm.Search="olympus";Check(Count(vm)==1,"search matches friendly map");
   vm.Search="not there";Check(Count(vm)==0,"search empty state");
   vm.Search="";vm.Filter="Multiplayer";Check(Count(vm)==1,"multiplayer excludes unknown responses");
   vm.Filter="All";vm.Search="192.0.2.3";Check(Count(vm)==1,"search matches endpoint");
   vm.Search="";var timestamp=vm.Updated;
   current=new MasterBrowserResult {Error="Timed out",CompletedUtc=when.AddMinutes(5)};
   vm.RefreshAsync().GetAwaiter().GetResult();
   Check(vm.Rows.Count==3&&vm.Updated==timestamp&&vm.Status.Contains("Previous results"),"failed refresh retains prior rows and timestamp explicitly");
   var waiting=new TaskCompletionSource<MasterBrowserResult>();
   work=(h,t)=>{calls++;t.Register(()=>waiting.TrySetCanceled());return waiting.Task;};
   int before=calls;var pending=vm.RefreshAsync();
   Check(vm.Busy&&!vm.RefreshCommand.CanExecute(null)&&vm.CancelCommand.CanExecute(null),"pending refresh exposes cancellable busy state");
   vm.RefreshAsync().GetAwaiter().GetResult();Check(calls==before+1,"overlapping refresh does not start another query");
   vm.CancelCommand.Execute(null);pending.GetAwaiter().GetResult();
   Check(!vm.Busy&&vm.Rows.Count==3&&vm.Status.Contains("cancelled"),"cancellation keeps prior results");
   var late=new TaskCompletionSource<MasterBrowserResult>();
   work=(h,t)=>late.Task;
   pending=vm.RefreshAsync();vm.Dispose();late.SetResult(new MasterBrowserResult {CompletedUtc=when.AddHours(1)});
   pending.GetAwaiter().GetResult();
   Check(vm.Rows.Count==3&&!vm.RefreshCommand.CanExecute(null),"late result after close cannot replace rows");
   var empty=new MasterBrowserViewModel((h,t)=>Task.FromResult(new MasterBrowserResult {CompletedUtc=when}));
   empty.RefreshAsync().GetAwaiter().GetResult();
   Check(empty.Rows.Count==0&&empty.Status.Contains("empty"),"successful empty master list is distinct from failure");
   empty.Dispose();return 0;
  } catch(Exception e) {Console.Error.WriteLine(e);return 1;}
 }
}
