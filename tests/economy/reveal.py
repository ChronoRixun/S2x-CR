"""Execute the extracted Zombies reveal coroutine with old and production replies.

Game assets are not included. Requires lupa with Lua 5.1.
"""
import argparse
import json
from pathlib import Path
import sys

p = argparse.ArgumentParser()
p.add_argument('--lupa-path', type=Path)
p.add_argument('--research-root', required=True, type=Path)
p.add_argument('--response', required=True, type=Path,
               help='zombies-drop-response.json emitted by the C++ economy test')
a = p.parse_args()
if a.lupa_path:
    sys.path.insert(0, str(a.lupa_path))
from lupa.lua51 import LuaRuntime

source = (a.research_root / 'luafiles/dec/ui_utility_mp_character_scene_supplydrop.dec.lua').read_text()
source = source[:source.index('Character_Scene.SupplyDropOpenSequence =')]
receipt = json.loads(a.response.read_text())
items = [f"0x{item['id']:x}" for item in receipt['GrantedItems']]
assert len(items) == 5
consumables = set(items[2:])

def run(guids):
    lua = LuaRuntime(unpack_returned_tuples=True)
    lua.execute('''
errors, flips, completed = {}, {}, false
Character_Scene = {SupplyDropControllerIndex=0, currentSupplyDropFXRarities={}}
Cac = {InvalidGuid="0x0"}
Game = {GetPlayerClientnum=function() return 0 end}
Lobby = {GetSecondsToNextMatch=function() return nil end}
Engine = {
 Loot_OpenSupplyPackage=function() return "test-tx" end,
 GetDvarInt=function() return 15 end, NotifyServer=function() end,
 Inventory_GetItemRarity=function() return 0 end,
 PlaySound=function() return 0 end, Localize=function(s) return s end
}
CharacterScene = {PlayFXOnTag=function() end, KillFXOnTag=function() end}
ModalUtils = {NotificationModalType={GeneralNotifications=1}}
LUI = {FlowManager={RequestAddMenu=function(_,_,_,_,_,options) errors[#errors+1]=options.titleText end},
 UITimer={new=function() return {addEventHandler=function() end} end, Reset=function() end, Enable=function() end, Disable=function() end}}
local function model()
 return {GetDataSourceForSubmodel=function(self,key) if not self[key] then self[key]=model() end return self[key] end,
 SetValue=function(self,controller,value) self.value=value end,
 GetValue=function(self) return self.value end}
end
DataSources={inGame={HUD={supplyDropCards=model()}}}
menu={addElement=function() end, wait=function() return {} end,
 processEvent=function(self,event) if event.name=="supply_drop_all_cards_revealed" then completed=true end end}
''')
    lua.globals().Engine.Inventory_IsItemGuidAZMConsumable = lambda guid: guid in consumables
    functions = lua.execute(source + '\nreturn {run=f0_local23, flip=f0_local9}')
    lua.globals().sequence = functions.run
    lua.globals().flip = functions.flip
    lua.globals().contents = lua.table_from([lua.table_from(dict(guid=id)) for id in guids])
    lua.execute('''
local thread = coroutine.create(sequence)
Character_Scene.SupplyDropSequenceThread=thread
for step=1,32 do
 local ok,err
 if step==1 then ok,err=coroutine.resume(thread,menu,0,3,false)
 else ok,err=coroutine.resume(thread) end
 assert(ok,err)
 -- Model a successful native result delivered while the crate animation runs.
 if step==1 then Character_Scene.currentSupplyDropItems=contents end
 local anim=Character_Scene.currentSupplyDropAnim
 if anim=="mp_hub_crate_card_2_out" or anim=="mp_hub_crate_card_4_out" or anim=="mp_hub_crate_card_6_out" then
  Character_Scene.OldSupplyDropAnim=anim
  flip({controllerIndex=0})
  flips[#flips+1]=anim
 end
 if #errors>0 or coroutine.status(thread)=="dead" then break end
end
''')
    return lua.globals()

old = run(items[2:])
assert len(old.errors) == 1 and old.errors[1] == 'HUB_SUPPLYDROP_TX_ERROR'
assert not old.completed
fixed = run(items)
assert len(fixed.errors) == 0 and fixed.completed
# Two regular flips, then three consumable flips (ignore repeats at wait yields).
assert fixed.flips[1] == 'mp_hub_crate_card_2_out' and fixed.flips[2] == 'mp_hub_crate_card_4_out'
assert fixed.flips[len(fixed.flips)] == 'mp_hub_crate_card_6_out'
print('PASS: old three-consumable response reproduces reveal error; production 2+3 response completes both stages')
