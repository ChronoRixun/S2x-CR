local root=Engine.GetLuiRoot();if not root then return end
local tick=0;local phase="start";local at=0;local c=Engine.GetFirstActiveController();local before;local xp
local timer=LUI.UITimer.new(1000,{name="cr_qa_zm",dispatchChildren=false},nil,false,false,false,false);timer.timerEventTarget=timer
local function qty(id)return tonumber((Engine.Inventory_GetItemQuantity(c,id,false))) or 0 end
local function step(value)phase=value;at=tick;print("CR_QA ZM_PHASE "..value)end
root:addEventHandler("OpenSupplyPackageSuccess",function(e,event)print("CR_QA ZM_OPEN_SUCCESS "..tostring(event.transactionID));local items=Engine.Loot_GetSupplyPackageContent(c,event.transactionID);for _,item in ipairs(items or {}) do print("CR_QA ZM_OPEN_ITEM "..tostring(item.guid).." quantity="..tostring(item.quantity)) end end)
root:addEventHandler("OpenSupplyPackageFailure",function(e,event)print("CR_QA ZM_OPEN_FAILURE")end)
timer:addEventHandler("cr_qa_zm",function()
 tick=tick+1;local ok,err=pcall(function()
 local info=root.flowManager and LUI.FlowManager.GetTopMenuInfo(root.flowManager.menuInfoStack);local menu=info and info.menu
 if menu and menu.id=="eula_menu" then print("CR_QA ZM_EULA_WAIT obtain human approval before accepting");return end
 if phase=="start" and tick>=5 and Engine.InFrontend() then Engine.SetDvarBool("master_server_enable",false);Engine.SetOnlineGame(true);Engine.SetSystemLink(false);Engine.CacheUserDataForController(c);Engine.Exec("xstartprivateparty");Engine.Inventory_FetchAllSKUs(c,SKUType.Quartermaster);step("fetch") end
 if phase=="fetch" and tick-at>5 and Engine.Inventory_AreSKUsFetched() then print("CR_QA ZM_SKUS "..tostring(#Engine.Inventory_GetAllSKUIDs(SKUType.Quartermaster)).." mode="..tostring(Engine.IsZombiesMode()));Engine.Exec("hqgrant currency 6 5000");step("wallet") end
 if phase=="wallet" and tick-at>4 and Engine.Inventory_GetCurrencyBalance(c,6)>=5000 then print("CR_QA ZM_BUY wallet="..tostring((Engine.Inventory_GetCurrencyBalance(c,6))).." tx="..tostring((Engine.Inventory_PurchaseSKU(c,"0x6",1))));step("buy") end
 if phase=="buy" and tick-at>4 and qty("0x6")>0 then print("CR_QA ZM_OPEN before="..qty("0x6").." tx="..tostring((Engine.Loot_OpenSupplyPackage(c,3))));step("open") end
 if phase=="open" and tick-at>6 then print("CR_QA ZM_OPEN_RESULT rare="..qty("0x6").." mpRare="..qty("0x2").." wallet="..tostring((Engine.Inventory_GetCurrencyBalance(c,6))));xp=tonumber(Engine.TableLookup("mp/cp_rankTable.csv",0,"1",2));before=qty("0x6");print("CR_QA ZM_RANK_SETUP threshold="..tostring(xp).." XP="..tostring((Engine.GetPlayerData(c,CoD.StatsGroup.Coop,"totalXP"))).." rare="..before);if xp then Engine.SetPlayerData(c,CoD.StatsGroup.Coop,"totalXP",xp)end;step("rank") end
 if phase=="rank" and tick-at>6 then print("CR_QA ZM_RANK_EVENT "..tostring((AchievementEngineUtils.AE_SendComplexGameEvent(c,AEComplexEvents.RankUp,{AEGameExeValue.ZM,2,0}))));step("rankwait") end
 if phase=="rankwait" and tick-at>6 then print("CR_QA ZM_RANK_RESULT before="..tostring(before).." rare="..qty("0x6").." mpRare="..qty("0x2").." XP="..tostring((Engine.GetPlayerData(c,CoD.StatsGroup.Coop,"totalXP"))));AchievementEngineUtils.AE_SendComplexGameEvent(c,AEComplexEvents.RankUp,{AEGameExeValue.ZM,2,0});step("repeat") end
 if phase=="repeat" and tick-at>6 then print("CR_QA ZM_REPEAT rare="..qty("0x6").." mpRare="..qty("0x2"));step("done");timer:close()end
 if tick%10==0 then print("CR_QA ZM_STATE tick="..tick.." phase="..phase.." menu="..tostring(menu and menu.id).." online="..tostring((Engine.UserOnlineDataFetched(c))))end
 end);if not ok then print("CR_QA ZM_ERROR "..tostring(err))end;if tick>=300 then timer:close()end
end);root:addElement(timer);print("CR_QA ZM_INIT synthetic profile")
