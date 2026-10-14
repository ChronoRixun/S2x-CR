local root=Engine.GetLuiRoot();if not root then return end
local tick=0;local c=Engine.GetFirstActiveController();local before
local timer=LUI.UITimer.new(1000,{name="cr_qa_rank",dispatchChildren=false},nil,false,false,false,false);timer.timerEventTarget=timer
timer:addEventHandler("cr_qa_rank",function()
 tick=tick+1;local ok,err=pcall(function()
 if tick==3 then before=Engine.Inventory_GetItemQuantity(c,"0x2",false);local xp=tonumber(Engine.TableLookup("mp/rankTable.csv",0,"2",2));print("CR_QA RANK_SETUP before="..tostring(before).." threshold="..tostring(xp).." experience="..tostring((Engine.GetPlayerData(c,CoD.StatsGroup.Ranked,"experience"))));if xp then Engine.Exec("setPlayerDataInt experience "..xp) end end
 if tick==8 or tick==16 then local result=AchievementEngineUtils.AE_SendComplexGameEvent(c,AEComplexEvents.RankUp,{AEGameExeValue.MP,2,0});print("CR_QA RANK_EVENT tick="..tick.." result="..tostring(result)) end
 if tick==13 or tick==23 then print("CR_QA RANK_RESULT tick="..tick.." before="..tostring(before).." after="..tostring((Engine.Inventory_GetItemQuantity(c,"0x2",false))).." experience="..tostring((Engine.GetPlayerData(c,CoD.StatsGroup.Ranked,"experience")))) end
 end);if not ok then print("CR_QA RANK_ERROR "..tostring(err)) end;if tick>=25 then timer:close() end
end);root:addElement(timer);print("CR_QA RANK_INIT synthetic native stats/event fixture")
