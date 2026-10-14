local root=Engine.GetLuiRoot();if not root then return end
local c=Engine.GetFirstActiveController();local tick=0;local combat=false;local fightTick=0
local oldEvent=AchievementEngineUtils.AE_SendComplexGameEvent
AchievementEngineUtils.AE_SendComplexGameEvent=function(controller,event,params) if event==AEComplexEvents.RankUp then print("CR_QA MATCH_NATIVE_RANK mode="..tostring(params and params[1]).." rank="..tostring(params and params[2]).." flag="..tostring(params and params[3]))end;return oldEvent(controller,event,params)end
local timer=LUI.UITimer.new(1000,{name="cr_qa_match",dispatchChildren=false},nil,false,false,false,false);timer.timerEventTarget=timer
timer:addEventHandler("cr_qa_match",function()
 tick=tick+1;local ok,err=pcall(function()
 local xp=Engine.GetPlayerData(c,CoD.StatsGroup.Coop,"totalXP")
 if Engine.GetDvarString("master_server")~="qa_match_seeded" and Engine.InFrontend() and xp~=nil and Engine.UserOnlineDataFetched(c) then local target=tonumber(Engine.TableLookup("mp/cp_rankTable.csv",0,"2",2));if target then Engine.SetPlayerData(c,CoD.StatsGroup.Coop,"totalXP",target-1);Engine.SetDvarString("master_server","qa_match_seeded");print("CR_QA MATCH_SEED threshold="..target.." xp="..(target-1))end end
 if not Engine.InFrontend() then local player=Game.GetPlayerEntitynum();if player~=nil then fightTick=fightTick+1;if not combat then combat=true;Engine.ExecNow("set sv_cheats 1");Engine.Exec("god");Engine.Exec("+right");print("CR_QA MATCH_COMBAT automated native input, QA invulnerability requested")end;if fightTick%2==0 then Engine.Exec("+melee")else Engine.Exec("-melee")end;if fightTick>=25 then Engine.Exec("-right");Engine.Exec("-melee")end end end; if tick%5==0 then print("CR_QA MATCH_STATE tick="..tick.." frontend="..tostring(Engine.InFrontend()).." xp="..tostring(xp).." rare="..tostring((Engine.Inventory_GetItemQuantity(c,"0x6",false))))end
 end);if not ok then print("CR_QA MATCH_ERROR "..tostring(err))end;if tick>=600 then timer:close()end
end);root:addElement(timer);print("CR_QA MATCH_INIT")


