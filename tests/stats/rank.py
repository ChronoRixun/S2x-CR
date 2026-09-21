"""Compile production rank commands with a synthetic stats/wallet boundary.

Requires VS 2022 C++ tools and lupa (Lua 5.1). No game/profile is opened.
Optional extracted tables exercise all shipped rank thresholds without distributing them.
"""
import argparse
import csv
import os
from pathlib import Path
import subprocess
import sys

p = argparse.ArgumentParser()
p.add_argument('--msbuild', type=Path, default=Path(
    r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe'))
p.add_argument('--lupa-path', type=Path)
p.add_argument('--tables', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
source = (root / 'src/client/component/stats.cpp').read_text()
out = root / 'build/tests/stats'
out.mkdir(parents=True, exist_ok=True)


def function(signature):
    start = source.index(signature)
    # Functions used here end at namespace indentation; inner blocks do not.
    end = source.index('\n\t\t}', start) + len('\n\t\t}')
    return source[start:end]


production = '\n'.join(function(s) for s in (
    'bool parse_integer(', 'int get_rank_level_cap(', 'bool get_rank_experience(',
    'int get_level_for_experience(', 'bool apply_progression(',
    'void set_rank_command(', 'void set_prestige_command('))
tables = []
if a.tables:
    for filename in ('rankTable.csv', 'cp_ranktable.csv'):
        rows = list(csv.reader((a.tables / filename).open()))
        values = {int(r[0]): int(r[2]) for r in rows if r[0].isdigit()}
        tables.append([values[i] for i in range(len(values))])
else:
    tables = [[0, 4000, 8600, 13800, 19600, 26000], [0, 3299, 13744, 29260, 48709, 71372]]
table_cpp = ',\n'.join('{' + ','.join(map(str, t)) + '}' for t in tables)

harness = r'''
#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <iostream>
#ifdef NDEBUG
#error Rank regression checks require assertions
#endif
namespace console {
template<class... T> void error(const char*, T...) {}
template<class... T> void warn(const char*, T...) {}
template<class... T> void info(const char*, T...) {}
}
namespace command {
struct params {
 std::vector<const char*> args;
 int size() const { return static_cast<int>(args.size()); }
 const char* operator[](int i) const { return args[i]; }
};
}
constexpr unsigned ranked_stats_group = 0;
bool zombies{}, broken_wallet{}, invalid_path{};
int current_prestige{};
unsigned writes{};
std::map<std::string, int> stats;
namespace demonware::hq_economy {
struct state { std::map<uint8_t, uint32_t> currencies; } wallet;
state snapshot() { if (broken_wallet) throw std::runtime_error("unreadable"); return wallet; }
}
struct rank_table_info {
 int max_prestige{10}, max_rank_index{}, max_rank_index_final_prestige{};
 std::vector<int> minimum_experience;
};
std::vector<std::vector<int>> tables = { TABLES };
bool load_rank_table(const char*, rank_table_info& i) {
 i.minimum_experience = tables[zombies];
 i.max_rank_index_final_prestige = static_cast<int>(i.minimum_experience.size()) - 1;
 i.max_rank_index = std::min(zombies ? 44 : 54, i.max_rank_index_final_prestige);
 return true;
}
struct progression_target { const char* table; const char* prestige_stat; const char* experience_stat; unsigned stats_group; };
bool resolve_progression_target(const char*, progression_target& t) {
 t = zombies ? progression_target{"zm", "prestigeLevel", "totalXP", 3} : progression_target{"mp", "prestige", "experience", 0};
 return true;
}
bool read_current_prestige(const progression_target&, int& p) { p = current_prestige; return true; }
bool is_stat_path_valid(std::initializer_list<std::string_view>, unsigned) { return !invalid_path; }
bool set_stat(std::initializer_list<std::string_view> path, int value, unsigned) {
 stats[std::string(*path.begin())] = value; ++writes; return true;
}
PRODUCTION
int display_xp() { return stats["experience"] + std::max(0, stats["inventoryTotalXP"] - stats["inventoryXPAtLastReset"]); }
int main() {
 using namespace demonware::hq_economy;
 for (bool mode : {false, true}) {
  zombies = mode;
  rank_table_info info; load_rank_table("", info);
  for (int level = 1; level <= static_cast<int>(info.minimum_experience.size()); ++level) {
   wallet.currencies = {{1, 18000}, {6, 999}};
   stats = {{"inventoryTotalXP", 15000}, {"inventoryXPAtLastReset", 3000}};
   assert(apply_progression("test", 10, level));
   const int xp = mode ? stats["totalXP"] : display_xp();
   assert(xp == info.minimum_experience[level - 1]);
   assert(get_level_for_experience(info, xp) == level);
   assert(wallet.currencies.at(1) == 18000 && wallet.currencies.at(6) == 999);
   if (mode) assert(stats["inventoryTotalXP"] == 15000 && stats["inventoryXPAtLastReset"] == 3000);
   else {
    // The next native sync must not bring old reward XP back; new rewards still count.
    stats["inventoryTotalXP"] = wallet.currencies.at(1);
    assert(display_xp() == xp);
    stats["inventoryTotalXP"] += 100;
    assert(display_xp() == xp + 100);
   }
  }
 }
 zombies = false;
 wallet.currencies = {{1, 9000}};
 set_rank_command({{"setrank", "3"}});
 assert(display_xp() == 8600);
 set_rank_command({{"setrank", "2", "4"}});
 assert(display_xp() == 4000 && stats["prestige"] == 4);
 set_prestige_command({{"setprestige", "2"}});
 assert(display_xp() == 0 && stats["prestige"] == 2);
 wallet.currencies.clear();
 assert(apply_progression("test", 0, 1) && display_xp() == 0);
 rank_table_info info; load_rank_table("", info);
 assert(apply_progression("test", 0, 9999));
 assert(display_xp() == info.minimum_experience[info.max_rank_index]);
 assert(apply_progression("test", 99, -1) && display_xp() == 0 && stats["prestige"] == 10);
 auto before = stats; writes = 0;
 broken_wallet = true;
 assert(!apply_progression("test", 2, 3) && stats == before && writes == 0);
 broken_wallet = false; invalid_path = true;
 assert(!apply_progression("test", 2, 3) && stats == before && writes == 0);
 invalid_path = false; wallet.currencies = {{1, 0x80000000u}};
 assert(!apply_progression("test", 2, 3) && stats == before && writes == 0);
 set_rank_command({{"setrank", "invalid"}});
 assert(writes == 0);
 std::cout << "PASS: production rank commands, all MP/ZM thresholds, XP baseline, future rewards, guards\n";
}
'''.replace('TABLES', table_cpp).replace('PRODUCTION', production)
(out / 'rank.cpp').write_text(harness)
(out / 'rank.vcxproj').write_text('''<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
<ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
<PropertyGroup Label="Globals"><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
<PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v143</PlatformToolset></PropertyGroup>
<Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
<PropertyGroup><OutDir>$(ProjectDir)bin\\</OutDir><IntDir>$(ProjectDir)obj\\</IntDir></PropertyGroup>
<ItemDefinitionGroup><ClCompile><LanguageStandard>stdcpp20</LanguageStandard><ExceptionHandling>Sync</ExceptionHandling></ClCompile></ItemDefinitionGroup>
<ItemGroup><ClCompile Include="rank.cpp"/></ItemGroup><Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" /></Project>''')
env = {k.upper(): v for k, v in os.environ.items()}
subprocess.run([str(a.msbuild), str(out / 'rank.vcxproj'), '/p:Configuration=Release',
                '/p:Platform=x64', '/m:1', '/v:minimal', '/nologo'], env=env, check=True)
subprocess.run([str(out / 'bin/rank.exe')], check=True)

if a.lupa_path:
    sys.path.insert(0, str(a.lupa_path))
from lupa.lua51 import LuaRuntime
lua = LuaRuntime(unpack_returned_tuples=True)
ui = (root / 'data/ui_scripts/mp/patches/unlocks.lua').read_text()
reader = ui[ui.index('local function try_player_data('):ui.index('local function progression_options(')]
lua.execute('''
zombies = false
Engine = {
 GetPlayerData = function(c, g, field)
  if field == 'prestige' or field == 'prestigeLevel' then return 2 end
  return 4000
 end,
 GetPlayerDataMPXP = function(c, g) assert(g == 0); return 16000 end
}
S2xStats = {
 GetProgressionSource = function() if zombies then return 3, 'prestigeLevel', 'totalXP' end; return 0, 'prestige', 'experience' end,
 GetLevelForExperience = function(xp) last_xp = xp; return xp >= 13800 and 4 or 2 end
}
''')
read = lua.execute(reader + '\nreturn read_current_progression')
assert read(0) == (True, 2, 4) and lua.globals().last_xp == 16000
lua.globals().zombies = True
assert read(0) == (True, 2, 2) and lua.globals().last_xp == 4000
lua.globals().zombies = False
lua.execute('Engine.GetPlayerDataMPXP = function() error("not ready") end')
assert read(0) == (False, 0, 1)
print('PASS: production chooser reads total MP XP, preserves ZM source, handles unavailable reader')
