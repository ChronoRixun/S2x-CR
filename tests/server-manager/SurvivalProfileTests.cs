using System;
using System.Collections;
using System.Linq;
using System.Reflection;
class PickerCheck {
 static object Get(object o,string n){var p=o.GetType().GetProperty(n);return p!=null?p.GetValue(o,null):o.GetType().GetField(n).GetValue(o);}
 static void Set(object o,string n,object v){o.GetType().GetProperty(n).SetValue(o,v,null);}
 static void Require(bool v,string why){if(!v)throw new Exception(why);Console.WriteLine("PASS "+why);}
 [STAThread] static int Main(string[] args) {
  try {
   var asm=Assembly.LoadFrom(args[0]);
   var ft=asm.GetType("S2x.ServerManager.ViewModels.FleetViewModel",true);
   var lp=asm.GetType("S2x.ServerManager.Services.LaunchProfiles",true);
   bool missing=args[1]=="missing";
   string[] maps={"mp_zombie_windmill_srv","mp_zombie_dnk_srv","mp_zombie_dig_02_srv"};
   string[] ids={"zombies-bots-bodega","zombies-bots-olympus","zombies-bots-altar"};
   string[] keys={"bodega","olympus","altar"};
   for(int m=0;m<maps.Length;m++) for(int bots=2;bots<=3;bots++) {
    var fleet=ft.GetMethod("DemoEditor").Invoke(null,new object[]{"zombies"});
    var profiles=(IList)Get(fleet,"Profiles");
    for(int k=0;k<ids.Length;k++){var p=lp.GetMethod("Read").Invoke(null,new object[]{args[k+2]});Require(((IList)Get(p,"Entries")).Count==2,"profile loads both entries "+ids[k]);profiles.Add(p);}
    var editor=Get(fleet,"Editor");
    editor.GetType().GetMethod("ProfilesChanged").Invoke(editor,null);
    var map=((IEnumerable)Get(editor,"MapOptions")).Cast<object>().FirstOrDefault(x=>(string)Get(x,"Key")==maps[m]);
    if(missing){Require(map==null,"baseline picker cannot select "+maps[m]);continue;}
    Require(map!=null,"picker includes "+maps[m]);
    Set(editor,"PickedMap",map);
    var modes=((IEnumerable)Get(editor,"GametypeOptions")).Cast<object>().ToArray();
    Require(modes.Length==3,"survival map offers normal mode and both bot counts");
    var wanted="profile:"+ids[m]+":"+keys[m]+"-"+bots;
    var option=modes.Single(x=>(string)Get(x,"Key")==wanted);
    ((IList)Get(editor,"Rotation")).Clear();
    Set(editor,"PickedGametype",option);
    Get(editor,"AddCommand").GetType().GetMethod("Execute").Invoke(Get(editor,"AddCommand"),new object[]{null});
    Require((bool)Get(editor,"IsProfile"),"selection becomes profile server "+wanted);
    var preset=Activator.CreateInstance(asm.GetType("S2x.ServerManager.Models.ServerPreset"),true);
    editor.GetType().GetMethod("Apply",BindingFlags.Instance|BindingFlags.NonPublic).Invoke(editor,new[]{preset});
    Require((string)Get(preset,"LaunchProfileId")==ids[m] && (string)Get(preset,"LaunchEntryKey")==keys[m]+"-"+bots,"preset retains exact profile and bot count");
    var rotation=(IList)Get(preset,"Rotation");
    Require(rotation.Count==1 && (string)Get(rotation[0],"Map")==maps[m],"preset retains survival map");
    var baseMap=((IEnumerable)Get(editor,"MapOptions")).Cast<object>().Single(x=>(string)Get(x,"Key")==maps[m].Replace("_srv",""));
    Set(editor,"PickedMap",baseMap);
    Require(((IList)Get(editor,"GametypeOptions")).Count==1,"Tortured Path variant does not inherit survival bots");
   }
   return 0;
  } catch(Exception e){Console.Error.WriteLine(e);return 1;}
 }
}
