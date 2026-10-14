#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "game/demonware/reward/game_event.hpp"
#include "game/demonware/marketplace/store.hpp"

namespace cr_progression::policy
{
 struct cosmetic { std::string key; std::vector<std::uint32_t> items; };
 inline std::optional<std::uint64_t> parameter(const demonware::reward_game_events::event& event, const char* selector)
 {
  std::optional<std::uint64_t> result;
  for (const auto& p : event.parameters) if (p.selector == selector) { if (result) return {}; result = p.value; }
  return result;
 }
 inline bool unique(const demonware::reward_game_events::event& event)
 {
  for (std::size_t i = 0; i < event.parameters.size(); ++i)
   for (std::size_t j = 0; j < i; ++j) if (event.parameters[i].selector == event.parameters[j].selector) return false;
  return true;
 }
 // Immutable fulfillment counts retain the high water mark across rank downs.
 inline int earned(const int prestige, const int xp, const int threshold, const int max_prestige,
  const bool master)
 {
  if (prestige < 0 || prestige > max_prestige || xp < 0 || threshold < 0) return -1;
  if (master) return prestige == max_prestige && xp >= threshold ? 1 : 0;
  return prestige + (xp >= threshold ? 1 : 0);
 }
 enum class readiness { ready, wait, baseline, blocked };
 inline readiness migration_readiness(bool pending, bool native_stats_ready, bool frontend, bool gameplay, bool blocked)
 {
  if (!pending) return readiness::ready;
  if (blocked) return readiness::blocked;
  if (gameplay) return readiness::blocked;
  if (!native_stats_ready) return readiness::wait;
  return frontend ? readiness::baseline : readiness::wait;
 }
 template<class Record>
 bool baseline(Record& record, const int count)
 {
  if (count <= record.fulfilled_times) return false;
  record.fulfilled_times=count;return true;
 }
 // Used by the production atomic mutation and the offline store stub.
 template<class Record, class Grant>
 bool advance(Record& record, const int count, Grant&& grant)
 {
  if (count <= record.fulfilled_times) return false;
  const int delta = count - record.fulfilled_times;
  if (!grant(delta)) return false;
  record.fulfilled_times = count;
  return true;
 }
 template<class Transaction>
 inline bool grant_item(Transaction& state, const std::uint32_t id, const std::uint64_t user,
   const std::uint32_t now)
  {
   auto item = state.get_inventory(id).value_or(demonware::marketplace_store::inventory_record{});
   if ((item.player_id && item.player_id != user) || (!item.account_type.empty() && item.account_type != "steam") ||
    item.collision_field || !demonware::marketplace_store::is_permanent(item)) return false;
   if (item.quantity) return true;
   item.item_id=id; item.player_id=user; item.account_type="steam"; item.quantity=1; item.mod_date_time=now;
   return state.set_inventory(item) == demonware::marketplace_store::edit_result::updated;
  }
 inline std::vector<cosmetic> cosmetics(const demonware::reward_game_events::event& event, const bool zombies)
 {
  std::vector<cosmetic> result;
  if (!unique(event)) return result;
  if (event.name == "vendor")
  {
   if (zombies)
   {
    const auto level = parameter(event, "5");
    if (level && *level >= 1 && *level <= 10)
     for (std::uint32_t n = 1; n <= *level; ++n) result.push_back({"prestige:zm:" + std::to_string(n), {0x240025B + n}});
    return result;
   }
   const auto level = parameter(event, "2");
   if (level && *level >= 1 && *level <= 10)
   {
    const auto n = static_cast<std::uint32_t>(*level);
    result.push_back({"prestige:mp:" + std::to_string(n), {0x663210B + n, 0x28000E2 + n}});
   }
   struct division { std::uint64_t skill; const char* name; std::array<std::uint32_t, 3> items; };
   constexpr std::array divisions{
    division{1,"infantry",{0x280004F,0x6002001,0x101F200}}, division{2,"airborne",{0x2800050,0x6002003,0x1013200}},
    division{3,"armored",{0x2800052,0x6002009,0x101C200}}, division{4,"mountain",{0x2800051,0x600200D,0x1016200}},
    division{5,"expeditionary",{0x2800053,0x6002010,0x1022200}}, division{11,"resistance",{0x2800100,0x600201E,0x102D100}},
    division{13,"grenadier",{0x2800119,0x6032032,0x1082100}}, division{15,"commando",{0x280010D,0x603003A,0x1096102}}};
   const auto skill = parameter(event,"1"), rank = parameter(event,"3");
   if (rank && *rank >= 2 && *rank <= 4)
    for (const auto& d : divisions) if (skill == d.skill)
     for (std::size_t n=2;n<=*rank;++n) result.push_back({"prestige:division:" + std::string{d.name} + ":" + std::to_string(n),{d.items[n-2]}});
  }
  if (!zombies && event.name == "player_rank_up" && parameter(event,"2") == 1 && parameter(event,"3") == 1)
  {
   const auto index = parameter(event,"1");
   if (!index || *index <= 54 || *index >= 1000) return result;
   struct milestone { unsigned level; std::array<std::uint32_t,2> items; };
   // Exact CR policy; 56's camo/helmet placement remains CR's documented inference.
   constexpr std::array milestones{milestone{56,{0x704001E,0x6632116}},milestone{100,{0x6000039}},
    milestone{200,{0x6021039,0x7040019}},milestone{300,{0x6003039}},milestone{400,{0x6010039,0x704001A}},
    milestone{500,{0x6011039}},milestone{600,{0x6023039,0x704001B}},milestone{666,{0x704001F}},
    milestone{700,{0x6033039}},milestone{800,{0x6013039,0x704001C}},milestone{900,{0x6020039}},milestone{1000,{0x6030039,0x704001D}}};
   for (const auto& m : milestones) if (*index + 1 >= m.level)
   {
    cosmetic reward{"prestige:master:"+std::to_string(m.level),{}};
    for (const auto item : m.items) if (item) reward.items.push_back(item);
    result.push_back(std::move(reward));
   }
  }
  return result;
 }
}
