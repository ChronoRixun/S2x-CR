#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "cr_progression.hpp"
#include "cr_progression_policy.hpp"
#include "economy.hpp"
#include "component/achievement_sync.hpp"
#include "component/console/console.hpp"
#include "component/scheduler.hpp"
#include "game/game.hpp"
#include "game/string_table.hpp"
#include "game/demonware/achievement/store.hpp"
#include "game/demonware/loot/catalog.hpp"
#include "game/demonware/loot/supply_drop_inventory.hpp"
#include "game/demonware/runtime_context.hpp"

namespace cr_progression
{
 namespace
 {
  using namespace demonware;
  struct rank { int selector; int xp; bool master; };
  struct definitions { std::vector<rank> ranks; int max_prestige{}; std::uint32_t drop{}; };
  std::atomic<std::shared_ptr<const definitions>> current;
  bool load_definitions()
  {
   using namespace game::string_table;
   const bool zm = game::environment::is_zombies();
   const auto catalog = loot_catalog::get_snapshot();
   if (!catalog) return false;
   const auto drop = loot_catalog::find_supply_drop(*catalog, zm ? "sd_zombie_rare" : "sd_mp_rare");
   if (!drop) return false;
   const auto* table = game::DB_FindXAssetHeader(game::ASSET_TYPE_STRINGTABLE,
    zm ? "mp/cp_rankTable.csv" : "mp/rankTable.csv", false).stringTable;
   auto result = std::make_shared<definitions>();
   int maximum{}, final{};
   if (!table || !parse_integer(get_cell(table,find_row(table,0,"maxprestige"),1),result->max_prestige) ||
    !parse_integer(get_cell(table,find_row(table,0,"maxrankfinalprestige"),1),final) ||
    result->max_prestige < 0 || result->max_prestige > 100 || final < 0 || final > 999 || final >= table->rowCount) return false;
   const auto regular_row=find_row(table,0,"maxrank");
   maximum=final; // CR supports tables without a separate ordinary-rank cap.
   if (regular_row >= 0 && (!parse_integer(get_cell(table,regular_row,1),maximum) || maximum < 0 || maximum > final)) return false;
   for (int index=0;index<=final;++index)
   {
    const auto row = find_row(table,0,std::to_string(index));
    int xp{}, selector = index;
    if (!parse_integer(get_cell(table,row,2),xp) || xp < 0 ||
     (zm && (!parse_integer(get_cell(table,row,12),selector) || selector < 0))) return false;
    if (!result->ranks.empty() && xp < result->ranks.back().xp) return false;
    result->ranks.push_back({selector,xp,index>maximum});
   }
   result->drop=drop->item_id;
   if (catalog != loot_catalog::get_snapshot()) return false;
   current.store(std::move(result));
   return true;
  }
  bool settle_cosmetic(const policy::cosmetic& reward, const std::uint64_t user,
   const loot_catalog::catalog& catalog, const std::uint32_t now)
  {
   // Migration preserves CR's settled identity with this explicit fingerprint.
   if (const auto receipt = marketplace_store::find_transaction(reward.key);
    receipt && receipt->request_fingerprint == "migration:cr-settled") return true;
   for (const auto id : reward.items)
    if (std::ranges::find(catalog.items,id,[](const auto& row){return row.item.item_id;}) == catalog.items.end()) return false;
   const auto result = marketplace_store::transact(reward.key,"cr-progression:"+reward.key,
    [&](marketplace_store::transaction& state,std::string& response)
    {
     for (const auto id : reward.items) if (!policy::grant_item(state,id,user,now)) return false;
     response="{}"; return true;
    });
   if (result.status != marketplace_store::transaction_status::committed &&
    result.status != marketplace_store::transaction_status::replayed) return false;
   economy::request_inventory_refresh();
   return true;
  }
  bool migration_pending(const bool zm)
  {
   if (!achievement_store::is_available()) return true;
   const auto records=achievement_store::get_all();
   const auto marker=std::ranges::find(records,"cr_progression_rebaseline",&achievement_record::name);
   const auto done=std::string{zm ? "cr_progression_rebaseline_zm" : "cr_progression_rebaseline_mp"};
   return marker != records.end() && std::ranges::find(records,done,&achievement_record::name) == records.end();
  }
  bool rebaseline(const definitions& data, const runtime_context::zombie_rank& profile, const bool zm)
  {
   const auto done=std::string{zm ? "cr_progression_rebaseline_zm" : "cr_progression_rebaseline_mp"};
   for (const auto& row : data.ranks)
   {
    const int count=policy::earned(profile.prestige,profile.experience,row.xp,data.max_prestige,row.master);
    if (count < 0) return false;
    if (!count) continue;
    if (!zm && row.master)
    {
     if (achievement_store::mutate("player_master_prestige_ranks",[&](achievement_record& record)
     {
      if (record.kind == 5 && record.progress >= row.selector+1) return false;
      record.kind=5;record.progress=static_cast<std::uint16_t>(row.selector+1);record.progress_target=1000;
      record.status=achievement_status::in_progress;return true;
     }) == achievement_store::mutation_result::save_failed) return false;
     continue;
    }
    const auto name=std::string{zm ? "cr_zm_rank_" : "cr_mp_rank_"}+std::to_string(row.selector);
    if (achievement_store::mutate(name,[&](achievement_record& record)
    {
     if (record.kind != 5) {record.kind=5;record.fulfilled_times=0;}
     if (!policy::baseline(record,count)) return false;
     record.progress=1;record.progress_target=1;
     record.status=achievement_status::finished;return true;
    }) == achievement_store::mutation_result::save_failed) return false;
   }
   if (achievement_store::mutate(done,[](achievement_record& record)
   {
    record.kind=5;record.fulfilled_times=1;record.progress=1;record.progress_target=1;
    record.status=achievement_status::finished;return true;
   }) == achievement_store::mutation_result::save_failed) return false;
   return true;
  }
  bool startup_readiness()
  {
   const bool zm=game::environment::is_zombies();
   const bool pending=migration_pending(zm);
   const auto identity=runtime_context::get_snapshot();
   const auto profile=identity ? (zm ? identity->zombies : identity->multiplayer) : std::nullopt;
   // This callback runs only on main. A migration must settle from idle native
   // stats before gameplay; never use an already-advanced rank event as baseline.
   static bool blocked{};
   const bool lobby=game::virtual_lobby_loaded();
   const bool in_game=game::CL_IsLocalClientInGame(0);
   const auto* server=game::Dvar_FindMalleableVar("sv_running");
   // party.cpp's waiting_for_frontend gate establishes this idle state.
   const bool frontend=lobby || (!in_game && !(server && server->current.enabled) &&
    !game::SV_Loaded() && *game::frontend_state != 0);
   const bool gameplay=in_game && !lobby;
   const auto action=policy::migration_readiness(pending,identity && identity->user_id && profile.has_value(),
    frontend,gameplay,blocked);
   if (action == policy::readiness::ready) return current.load() || load_definitions();
   if (action == policy::readiness::blocked)
   {
    if (!blocked) console::error("[CR progression] Migration baseline not finalized before gameplay; rank rewards remain blocked. Restart in frontend before playing.\n");
    blocked=true;return false;
   }
   if (action == policy::readiness::wait) return false;
   // Gameplay detection above must run even when loot/rank definitions are unavailable.
   if (!current.load() && !load_definitions()) return false;
   return rebaseline(*current.load(),*profile,zm);
  }
  bool settle_rank(const reward_game_events::event& event, const std::uint64_t user, const bool zm,
   const std::uint32_t now)
  {
   const auto index=policy::parameter(event,"1"), mode=policy::parameter(event,"2"), flag=policy::parameter(event,"3");
   if (!index || mode != (zm ? 2u : 1u) || (flag && *flag > 1) || (zm && flag && *flag)) return true;
   const auto data=current.load();
   const auto identity=runtime_context::get_snapshot();
   if (!data || !identity || identity->user_id != user) return false;
   const auto profile=zm ? identity->zombies : identity->multiplayer;
   if (!profile) return false;
   if (migration_pending(zm)) return false; // Retry; readiness alone may finalize migration.
   const auto row=std::ranges::find(data->ranks,*index,&rank::selector);
   if (row == data->ranks.end()) return true;
   // HQ's flagged master replay includes the ordinary rank indices. CR uses
   // these only to advance AE84; they do not award ordinary rank drops.
   if (!zm && flag == 1 && !row->master)
   {
    return achievement_store::mutate("player_master_prestige_ranks",[&](achievement_record& record)
    {
     if (record.kind == 5 && record.progress >= *index+1) return false;
     record.kind=5;record.progress=static_cast<std::uint16_t>(*index+1);record.progress_target=1000;
     record.status=achievement_status::in_progress;return true;
    }) != achievement_store::mutation_result::save_failed;
   }
   const int count=policy::earned(profile->prestige,profile->experience,row->xp,data->max_prestige,row->master);
   if (count < 0) return false;
   if (!count) return false; // Publication may follow the native event; permit retry.
   int grants{};
   const auto name = !zm && row->master ? "player_master_prestige_ranks" :
    std::string{zm ? "cr_zm_rank_" : "cr_mp_rank_"}+std::to_string(*index);
   const auto result=achievement_store::mutate(name,[&](achievement_record& record)
   {
    if (record.kind != 5) {record.kind=5;record.fulfilled_times=count-1;record.progress=0;}
    if (!zm && row->master)
    {
     if (record.progress >= *index+1) return false;
     record.progress=static_cast<std::uint16_t>(*index+1); record.progress_target=1000;
     record.status=achievement_status::in_progress; grants=1;
    }
    else
    {
     if (!policy::advance(record,count,[&](const int delta){grants=delta;return true;})) return false;
     record.progress=1;record.progress_target=1;record.status=achievement_status::finished;
    }
    record.completion_timestamp=now;return true;
   },[&](marketplace_store::transaction& state)
   {
    marketplace_store::inventory_record item;
    for(int n=0;n<grants;++n) if(!supply_drop_inventory::grant(state,data->drop,user,now,item)) return false;
    return true;
   });
   if (result == achievement_store::mutation_result::save_failed) return false;
   if (result == achievement_store::mutation_result::updated) achievement_sync::request_refresh();
   economy::request_inventory_refresh();return true;
  }
 }
 bool process(const demonware::reward_game_events::event& event, const std::uint64_t user)
 {
  if (!user || game::environment::is_dedicated() ||
   (event.name != "vendor" && event.name != "player_rank_up")) return true;
  if (!policy::unique(event)) return true;
  const bool zm=game::environment::is_zombies();
  const auto now=static_cast<std::uint32_t>(time(nullptr));
  if (event.name == "player_rank_up" && !settle_rank(event,user,zm,now)) return false;
  const auto rewards=policy::cosmetics(event,zm);
  if (rewards.empty()) return true;
  const auto catalog=demonware::loot_catalog::get_snapshot();
  if (!catalog) return false;
  for (const auto& reward : rewards) if (!settle_cosmetic(reward,user,*catalog,now)) return false;
  return true;
 }
 class component final : public multiplayer_component
 {
 public:
  void post_unpack() override
  {
   if (!game::environment::is_dedicated()) scheduler::schedule(startup_readiness,scheduler::pipeline::main,1s);
  }
 };
}
REGISTER_COMPONENT(cr_progression::component)
