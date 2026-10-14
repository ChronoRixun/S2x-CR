#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include "component/demonware/cr_progression_policy.hpp"

using namespace demonware;
namespace policy = cr_progression::policy;
struct record { int fulfilled_times{}; };
struct transaction
{
 std::map<std::uint32_t,marketplace_store::inventory_record> inventory;
 bool fail{};
 std::optional<marketplace_store::inventory_record> get_inventory(std::uint32_t id) const
 { const auto i=inventory.find(id);return i==inventory.end()?std::nullopt:std::optional{i->second}; }
 marketplace_store::edit_result set_inventory(const marketplace_store::inventory_record& item)
 { if(fail) return marketplace_store::edit_result::capacity_exceeded;inventory[item.item_id]=item;return marketplace_store::edit_result::updated; }
};
// The stub commits both counters and inventory only after all reward callbacks succeed.
struct store
{
 record rank;
 transaction state;
 std::set<std::string> receipts;
 bool advance(int earned)
 {
  auto proposed=rank;auto inventory=state;
  const auto changed=policy::advance(proposed,earned,[&](int delta)
  {
   if(inventory.fail) return false;
   auto item=inventory.get_inventory(2).value_or(marketplace_store::inventory_record{});
   item.item_id=2;item.quantity+=delta;return inventory.set_inventory(item)==marketplace_store::edit_result::updated;
  });
  if(changed){rank=proposed;state=inventory;}return changed;
 }
 bool cosmetic(const policy::cosmetic& reward)
 {
  if(receipts.contains(reward.key)) return true;
  auto proposed=state;
  for(auto id:reward.items) if(!policy::grant_item(proposed,id,123,1)) return false;
  state=proposed;receipts.insert(reward.key);return true;
 }
};
reward_game_events::event event(std::string name, std::initializer_list<reward_game_events::parameter> p)
{return {std::move(name),123,p};}
int main()
{
 // Migration settles in idle readiness, before the native profile advances.
 assert(policy::migration_readiness(true,false,true,false,false)==policy::readiness::wait);
 assert(policy::migration_readiness(true,true,true,false,false)==policy::readiness::baseline);
 assert(policy::migration_readiness(true,true,false,false,false)==policy::readiness::wait); // Loading has neither idle nor gameplay proof.
 assert(policy::migration_readiness(true,true,false,true,false)==policy::readiness::blocked);
 assert(policy::migration_readiness(true,true,true,false,true)==policy::readiness::blocked);
 assert(policy::migration_readiness(false,true,false,true,false)==policy::readiness::ready);
 assert(policy::migration_readiness(true,false,false,true,false)==policy::readiness::blocked); // Gameplay before stats publication.
 // Table loading happens only after the gameplay latch in production. Model
 // that order: unready tables cannot hide gameplay, nor clear its later latch.
 bool tables_ready=false;
 bool gameplay_blocked=policy::migration_readiness(true,false,false,true,false)==policy::readiness::blocked;
 assert(!tables_ready && gameplay_blocked);
 tables_ready=true;
 assert(policy::migration_readiness(true,true,true,false,gameplay_blocked)==policy::readiness::blocked);
 store migrated;
 assert(policy::baseline(migrated.rank,policy::earned(1,0,100,10,false)));
 assert(migrated.state.inventory.empty()); // Startup baseline must never grant.
 assert(!migrated.advance(policy::earned(1,0,100,10,false))); // Historical replay.
 assert(migrated.advance(policy::earned(1,100,100,10,false))); // FIRST actual rank-up pays.
 assert(migrated.state.inventory.at(2).quantity==1);
 assert(!migrated.advance(policy::earned(1,100,100,10,false))); // Its replay cannot repay.
 store s;
 assert(s.advance(policy::earned(0,100,100,10,false)));
 assert(s.state.inventory.at(2).quantity==1);
 assert(!s.advance(policy::earned(0,100,100,10,false))); // New transport ID and timestamp cannot repay.
 assert(!s.advance(policy::earned(0,0,100,10,false))); // Rankdown retains high water.
 assert(!s.advance(policy::earned(0,100,100,10,false))); // Rebaseline/re-earn retains high water.
 assert(s.advance(policy::earned(1,100,100,10,false))); // Legitimate next prestige.
 s.state.fail=true;
 assert(!s.advance(policy::earned(2,100,100,10,false)));
 assert(s.rank.fulfilled_times==2 && s.state.inventory.at(2).quantity==2);
 s.state.fail=false;
 assert(s.advance(policy::earned(2,100,100,10,false)));
 assert(policy::earned(-1,100,100,10,false)==-1);
 assert(policy::earned(11,100,100,10,false)==-1);
 assert(policy::earned(9,100,100,10,true)==0);
 assert(policy::earned(10,100,100,10,true)==1);
 const auto zm=policy::cosmetics(event("vendor",{{"5",10}}),true);
 assert(zm.size()==10);
 for(const auto& r:zm){assert(s.cosmetic(r));assert(s.cosmetic(r));}
 assert(s.receipts.size()==10);
 for(const auto& r:zm) assert(s.state.inventory.at(r.items.front()).quantity==1);
 assert(policy::cosmetics(event("vendor",{{"5",11}}),true).empty());
 assert(policy::cosmetics(event("vendor",{{"2",5}}),true).empty());
 for(int n=1;n<=10;++n)
 {
  auto mp=policy::cosmetics(event("vendor",{{"2",static_cast<std::uint64_t>(n)}}),false);
  assert(mp.size()==1 && mp.front().items.size()==2);assert(s.cosmetic(mp.front()));assert(s.cosmetic(mp.front()));
 }
 for(const auto skill:{1,2,3,4,5,11,13,15})
 {
  const auto division=policy::cosmetics(event("vendor",{{"1",static_cast<std::uint64_t>(skill)},{"3",4}}),false);
  assert(division.size()==3);
  for(const auto& r:division){assert(s.cosmetic(r));assert(s.cosmetic(r));}
 }
 assert(policy::cosmetics(event("vendor",{{"1",99},{"3",4}}),false).empty());
 assert(policy::cosmetics(event("vendor",{{"1",1},{"3",5}}),false).empty());
 assert(policy::cosmetics(event("vendor",{{"2",1},{"2",2}}),false).empty());
 const auto master=policy::cosmetics(event("player_rank_up",{{"1",999},{"2",1},{"3",1}}),false);
 assert(master.size()==12);
 for(const auto& r:master){assert(s.cosmetic(r));assert(s.cosmetic(r));}
 assert(policy::cosmetics(event("player_rank_up",{{"1",999},{"2",1}}),false).empty());
 assert(policy::cosmetics(event("player_rank_up",{{"1",999},{"2",2},{"3",1}}),false).empty());
 transaction bad;
 marketplace_store::inventory_record item;item.item_id=100;item.quantity=1;item.player_id=999;bad.inventory[100]=item;
 assert(!policy::grant_item(bad,100,123,1));
 bad.inventory[100].player_id=123;bad.inventory[100].collision_field=1;
 assert(!policy::grant_item(bad,100,123,1));
 bad.inventory[100].collision_field=0;bad.inventory[100].expire_date_time=50;
 assert(!policy::grant_item(bad,100,123,1));
 store rollback;
 rollback.state.fail=true;
 assert(!rollback.cosmetic(master.front()));assert(rollback.receipts.empty() && rollback.state.inventory.empty());
 rollback.state.fail=false;assert(rollback.cosmetic(master.front()));
 assert(rollback.state.inventory.size()==2);
 // Migration's existing identity gate makes already-settled cosmetics a no-op.
 rollback.receipts.insert(zm.front().key);assert(rollback.cosmetic(zm.front()));
 assert(!rollback.state.inventory.contains(zm.front().items.front()));
 std::cout << "PASS: rank replay/reset/prestige/rollback; all MP/ZM/division/master cosmetic rewards; collision/rental guards\n";
}
