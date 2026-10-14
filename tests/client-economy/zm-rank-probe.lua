local root=Engine.GetLuiRoot();if not root then return end
local tick=0;local stage=0;local at=0;local c=Engine.GetFirstActiveController();local before
local timer=LUI.UITimer.new(1000,{name="cr_qa_zm_rank",dispatchChildren=false},nil,false,false,false,false);timer.timerEventTarget=timer
local function qty(id)return tonumber((Engine.Inventory_GetItemQuantity(c,id,false))) or 0 end
local function setstage(value)stage=value;at=tick end
timer:addEventHandler("cr_qa_zm_rank",function()
 tick=tick+1;local ok,err=pcall(function()
 local currentXP=Engine.GetPlayerData(c,CoD.StatsGroup.Coop,"totalXP")
 if stage==0 and currentXP~=nil and Engine.UserOnlineDataFetched(c) then before=qty("0x6");local xp=tonumber(Engine.TableLookup("mp/cp_rankTable.csv",0,"1",2));print("CR_QA ZM_READY_RANK before="..before.." xp="..tostring(currentXP).." threshold="..tostring(xp));if xp then Engine.SetPlayerData(c,CoD.StatsGroup.Coop,"totalXP",xp);setstage(1)end end
 if stage==1 and tick-at>5 then print("CR_QA ZM_READY_EVENT "..tostring((AchievementEngineUtils.AE_SendComplexGameEvent(c,AEComplexEvents.RankUp,{AEGameExeValue.ZM,2,0}))));setstage(2)end
 if stage==2 and tick-at>5 then print("CR_QA ZM_READY_RESULT rare="..qty("0x6").." mpRare="..qty("0x2").." xp="..tostring(currentXP));AchievementEngineUtils.AE_SendComplexGameEvent(c,AEComplexEvents.RankUp,{AEGameExeValue.ZM,2,0});setstage(3)end
 if stage==3 and tick-at>5 then print("CR_QA ZM_READY_REPEAT rare="..qty("0x6").." mpRare="..qty("0x2").." xp="..tostring(currentXP));timer:close();setstage(4)end
 if tick%10==0 then print("CR_QA ZM_RANK_WAIT tick="..tick.." stage="..stage.." xp="..tostring(currentXP).." online="..tostring((Engine.UserOnlineDataFetched(c))))end
 end);if not ok then print("CR_QA ZM_READY_ERROR "..tostring(err))end;if tick>=180 then timer:close()end
end);root:addElement(timer);print("CR_QA ZM_READY_INIT synthetic stats/event fixture")
