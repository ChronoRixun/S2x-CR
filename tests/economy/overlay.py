"""Validate the embedded Lua policy; optionally check locally extracted game assets.

Requires lupa with Lua 5.1. No game assets are distributed with this test.
"""
import argparse
import csv
from pathlib import Path
import re
import sys

p = argparse.ArgumentParser()
p.add_argument('--lupa-path', type=Path)
p.add_argument('--research-root', type=Path)
a = p.parse_args()
if a.lupa_path:
    sys.path.insert(0, str(a.lupa_path))
from lupa.lua51 import LuaRuntime

root = Path(__file__).resolve().parents[2]
source = (root / 'src/client/component/hq_contracts.cpp').read_text()
policy = source.split('R"lua(', 1)[1].split(')lua"', 1)[0]
header = (root / 'src/client/game/demonware/hq_zombies_catalog.hpp').read_text()
entries = re.findall(r'\{(\d+), (\d+), "([^"]+)", (\d+), "([^"]+)", "([^"]+)", "([^"]+)"\}', header)
assert len(entries) == 9
contract_header = (root / 'src/client/game/demonware/hq_zombies_contract_catalog.hpp').read_text()
contracts = re.findall(r'\{(\d+), "([^"]+)", "([^"]+)", "([^"]+)", (\d+), (\d+), (0x[0-9A-Fa-f]+), (0x[0-9A-Fa-f]+), (\d+)\}', contract_header)
assert len(contracts) == 3

if a.research_root:
    with (a.research_root / 'tables/dwgamechallenges.csv').open() as f:
        retail = {r[0]: r for r in csv.reader(f)}
    for id, kind, name, target, title, description, predicate in entries:
        assert retail[id][1:5] == [name, kind, '34', predicate], id
    with (a.research_root / 'tables/periodicChallengeTable.csv').open() as f:
        periodic = {r[0]: r for r in csv.reader(f)}
    for id, kind, *_ in entries:
        assert periodic[id][1] == ('AEC_DAILY' if kind == '8' else 'AEC_WEEKLY')
    with (a.research_root / 'tables/StatsTable.csv').open() as f:
        tokens = {int(r[18], 16): r for r in csv.reader(f) if len(r) > 18 and r[18].startswith('0x')}
    for id, name, title, description, target, seconds, token, sku, price in contracts:
        assert retail[id][1:5] == [name, '11', '34', ''], id
        assert periodic[id][1] == 'AEC_CONTRACT' and periodic[id][8] == target
        assert periodic[id][10] == seconds and int(periodic[id][11], 16) == int(token, 16)
        assert tokens[int(token, 16)][0] == 'contract' and tokens[int(token, 16)][2] == name


for zombies in (False, True):
    lua = LuaRuntime(unpack_returned_tuples=True)
    lua.globals().zombies = zombies
    lua.execute('''
LUI = { SingleSplit = function(text, sep) return string.match(text, "^(.-)" .. sep .. "(.*)$") end }
CONDITIONS = { IsZombiesMode = function() return zombies end }
Engine = {
 TableLookup = function(...) return "stock" end,
 GetItemGUIDFromReference = function(...) return "0x123" end,
 AE_GetScheduledChallenges = function() return records end,
 AE_GetPlayerActiveChallenges = function() return records end
}
S2xZombiesOrders = {}
S2xZombiesContracts = {}
records = {{ ID = 10 }, { ID = 999999, reward = "untouched" }}
''')
    for index, (id, kind, name, target, title, description, _) in enumerate(entries, 1):
        lua.globals().S2xZombiesOrders[index] = lua.table_from(dict(
            id=int(id), kind=int(kind), name=name, target=int(target),
            title=title, description=description))
        lua.globals().records[index + 2] = lua.table_from(dict(ID=int(id)))
    for index, (id, name, title, description, target, seconds, token, sku, price) in enumerate(contracts, 1):
        lua.globals().S2xZombiesContracts[index] = lua.table_from(dict(
            id=int(id), name=name, title=title, description=description,
            target=int(target), seconds=int(seconds), token=token.lower()))
        lua.globals().records[index + 11] = lua.table_from(dict(ID=int(id)))
    lua.execute(policy)
    lookup = lua.globals().Engine.TableLookup
    file = 'mp/periodicChallengeTable.csv'
    for id, kind, name, target, *_ in entries:
        assert lookup(file, 0, int(id), 2) == (name if zombies else 'stock')
        if zombies:
            assert lookup(file, 0, int(id), 1) == ('AEC_DAILY' if kind == '8' else 'AEC_WEEKLY')
            assert lookup(file, 0, int(id), 8) == target
            for column in range(12, 20):
                assert lookup(file, 0, int(id), column) == ''
        assert lookup(file, 0, int(id), 1.5) == 'stock'
        assert lookup('other.csv', 0, int(id), 2) == 'stock'
    assert lookup(file, 0, 10, 2) == ('stock' if zombies else 'daily_ch_kills')
    for id, name, title, description, target, seconds, token, sku, price in contracts:
        assert lookup(file, 0, int(id), 2) == (name if zombies else 'stock')
        if zombies:
            assert lookup(file, 0, int(id), 1) == 'AEC_CONTRACT'
            assert lookup(file, 0, int(id), 10) == seconds
            assert lookup(file, 0, int(id), 11) == token.lower()
            for column in range(12, 20):
                assert lookup(file, 0, int(id), column) == ''

    for func in ('AE_GetScheduledChallenges', 'AE_GetPlayerActiveChallenges'):
        records = lua.globals().Engine[func](0)
        assert records[2].reward == 'untouched'
        for index, entry in enumerate(entries, 3):
            reward = records[index].reward
            if zombies and entry[1] == '8':
                assert reward.currencyID == 6 and reward.currencyAmount == 250
            elif zombies:
                assert reward.productID == '0x6' and reward.itemID == '0x6'
            else:
                assert reward is None
        for index, entry in enumerate(contracts, 12):
            if zombies:
                assert records[index].reward.productID == '0x6'
                assert records[index].timeLimit == int(entry[5])
            else:
                assert records[index].reward is None
    if zombies and a.research_root:
        utils = (a.research_root / 'luafiles/dec/ui_utility_mp_achievementengineutils.dec.lua').read_text()
        lua.execute(utils[:utils.index('AchievementEngineUtils.GetSpecialZMMaterialByMTX')])
        start = utils.index('local f0_local3 = function ( f25_arg0 )')
        end = utils.index('AchievementEngineUtils.GetConversionOverrideByID')
        lua.execute(utils[start:end])
        for entry in entries:
            assert lua.globals().AchievementEngineUtils.ShouldDisplayChallengeByID(0, int(entry[0]))
        for entry in contracts:
            assert lua.globals().AchievementEngineUtils.ShouldDisplayChallengeByID(0, int(entry[0]))
        # Run the actual inventory-tab grouping function: mode-specific periodic
        # strings previously passed visibility checks but dropped every accepted order.
        inventory = (a.research_root / 'luafiles/dec/ui_s2_periodicchallengeinventory_uc.dec.lua').read_text()
        group_source = inventory[inventory.index('local f0_local13 ='):inventory.index('local f0_local14 =')]
        lua.execute("""
CONDITIONS.IsInHubTutorial = function() return false end
DwDataUtils = { IsSpecialZombieChallenge = function() return false end }
AEPeriodicType = { Daily="AEC_DAILY", Weekly="AEC_WEEKLY", Contract="AEC_CONTRACT", Special="AEC_SPECIAL", Weapons="AEC_WEAPONS" }
AchievementEngineUtils.GetTypeByID = function(id) return Engine.TableLookup(AEChallengeTable.File, 0, id, 1) end
inventoryScope = { OrderTabMenu = { [0] = { playerActiveDailies={}, playerActiveWeeklies={}, playerActiveContracts={}, playerActiveSpecials={}, playerActiveWeapons={} } } }
""")
        group = lua.execute(group_source + 'return f0_local13')
        for entry in entries + contracts:
            group(lua.globals().inventoryScope, 0, lua.table_from(dict(ID=int(entry[0]))))
        grouped = lua.globals().inventoryScope.OrderTabMenu[0]
        assert len(grouped.playerActiveDailies) == 6 and len(grouped.playerActiveWeeklies) == 3
        assert len(grouped.playerActiveContracts) == 3
        # Exercise the extracted shared contract cache and price/token helpers.
        # This is not a claim that the unavailable ZM menu body was executed.
        lua.execute("""
AEPeriodicType = { Contract = "AEC_CONTRACT" }
GameChallengeGroup = { Scheduled = 1 }
DwDataUtils = { Vendor = { Operation = 1 }, GetCachedData = {
 function() return Engine.AE_GetScheduledChallenges(0) end } }
scope = { quartermaster = { SKUInfos = {} } }
""")
        for index, entry in enumerate(contracts, 1):
            lua.globals().scope.quartermaster.SKUInfos[index] = lua.table_from(dict(
                skuID=int(entry[7], 16),
                items=lua.table_from([lua.table_from(dict(guid=entry[6].lower()))]),
                prices=lua.table_from([lua.table_from(dict(value=int(entry[8])))])))
        menu = (a.research_root / 'luafiles/dec/ui_s2_contracts_menu_uc.dec.lua').read_text()
        funcs = lua.execute(menu[:menu.index('local f0_local10 =')] +
                            'return { cache=f0_local1, cell=f0_local2, price=f0_local8, sku=f0_local9 }')
        funcs.cache(0)
        for entry in contracts:
            token = funcs.cell(int(entry[0]), 11)
            assert funcs.price(None, token, lua.globals().scope) == int(entry[8])
            assert funcs.sku(None, token, lua.globals().scope) == int(entry[7], 16)

print('PASS: Lua 5.1 policy, MP/ZM separation, rewards, native identities, visibility, inventory-tab grouping, contract timers and price/token lookups' if a.research_root else
      'PASS: Lua 5.1 policy, MP/ZM separation and rewards (game assets not checked)')
