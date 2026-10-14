local root=Engine.GetLuiRoot();if not root then return end
local tick=0
local oldMaterial=RegisterMaterial
RegisterMaterial=function(name,...) if tostring(name):find("credits",1,true) or tostring(name):find("cod_points",1,true) then print("CR_QA MATERIAL "..tostring(name)) end return oldMaterial(name,...) end
local function stage()return Engine.GetDvarString("master_server")end
local function setstage(value)Engine.SetDvarString("master_server",value);print("CR_QA STAGE "..tostring(stage()))end
local function dump(element,depth)
 if not element or depth>9 then return end
 if element.getText then local ok,text=pcall(element.getText,element);if ok and text and tostring(text)~="" then print("CR_QA UI_TEXT "..tostring(element.id).."="..tostring(text):sub(1,85)) end end
 local child=element:getFirstChild();local n=0;while child and n<40 do dump(child,depth+1);child=child:getNextSibling();n=n+1 end
end
local timer=LUI.UITimer.new(1000,{name="cr_qa_shop_ui",dispatchChildren=false},nil,false,false,false,false)
timer.timerEventTarget=timer
timer:addEventHandler("cr_qa_shop_ui",function()
 tick=tick+1;local c=Engine.GetFirstActiveController()
 local ok,err=pcall(function()
  if tick==1 then Engine.SetDvarBool("master_server_enable",false);Engine.SetDvarString("master_server","boot") end; local state=stage();local info=root.flowManager and LUI.FlowManager.GetTopMenuInfo(root.flowManager.menuInfoStack);local menu=info and info.menu
  if state=="boot" and tick==3 then Engine.SetOnlineGame(true)Engine.SetSystemLink(false)Engine.CacheUserDataForController(c)Engine.Exec("xstartprivateparty") end
  if state=="boot" and tick==8 then Engine.Inventory_FetchAllSKUs(c,SKUType.Quartermaster)Engine.AE_FetchScheduledChallenges(c)Engine.AE_FetchPlayerActiveChallenges(c) end
  if state=="boot" and tick==15 then setstage("hq");root:dispatchEventToRoot({name="handle_fullhub",controller=c}) end
  if state=="hq" and Engine.IsHubMode() and Game.GetPlayerEntitynum()~=nil and Engine.UserOnlineDataFetched(c) then
   setstage("qm");local player=Game.GetPlayerEntitynum();Engine.NotifyServerKiosk("quartermaster_menu",0,player,player)
   LUI.FlowManager.RequestAddMenu(root,"quartermaster_menu",true,c,false,{male=Engine.GetDvarBool("spv_hub_special_qm")})
  end
  if state=="qm" and menu and menu.id=="quartermaster_menu" and Character_Scene.curVendor then
   print("CR_QA QM_READY");setstage("cwl");QuarterMasterUtils.HandleTabChange(menu,c,2)
  end
  if state=="cwl" and menu and menu.id=="quartermaster_cwl_menu" then
   print("CR_QA CWL_READY");dump(menu,0);setstage("done")
  end
  if stage()=="done" and tick==30 then local skus=Engine.Inventory_GetAllSKUIDs(SKUType.Quartermaster);print("CR_QA SKU_COUNT "..tostring(#skus));local found=0;for _,id in ipairs(skus) do local data=Engine.Inventory_GetSKUInfoSKUData(id);if data and tostring(data):find("CWL",1,true) then print("CR_QA CWL_CACHE "..id.." "..tostring(data));found=found+1 end end;print("CR_QA CWL_CACHE_COUNT "..found);local cached=DwDataUtils.GetCachedData[DwDataUtils.Vendor.Quartermaster](c);print("CR_QA CWL_LOOKUP "..tostring((QuarterMasterUtils.FindSKUIDByType(cached,"CWL_EF"))));dump(menu,0) end; if tick%10==0 then print("CR_QA UI_STATE tick="..tick.." stage="..tostring(stage()).." hub="..tostring(Engine.IsHubMode()).." player="..tostring(Game.GetPlayerEntitynum()).." menu="..tostring(menu and menu.id).." online="..tostring(Engine.UserOnlineDataFetched(c))) end
 end)
 if not ok then print("CR_QA UI_ERROR "..tostring(err)) end
 if tick>=180 then timer:close() end
end)
root:addElement(timer)
print("CR_QA UI_INIT stage="..tostring(stage()))
