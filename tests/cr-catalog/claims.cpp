#include <std_include.hpp>
#include "game/demonware/achievement/store.hpp"
#include "game/demonware/achievement/order_progress.hpp"
#include "game/demonware/marketplace/cr_catalog.hpp"
#include "game/demonware/marketplace/cwl.hpp"
#include <iostream>
#include <source_location>
#include <utils/cryptography.hpp>
#include "game/demonware/marketplace/purchase.hpp"
#include <fstream>
#include "../../src/client/game/demonware/achievement/orders.cpp"
namespace { std::filesystem::path profile; void check(bool ok, const std::source_location where=std::source_location::current()) { if (!ok) { std::cerr << "FAIL at line " << where.line() << "\n"; std::exit(1); } } }
namespace player_profile { const std::filesystem::path& user_directory() { return profile; } }
namespace utils::nt { std::string load_resource(int id) {
 std::string name;
 switch(id) {
 case DW_ACHIEVEMENT_OFFERS:name="achievement_offers.json";break;
 case DW_MARKETPLACE_CATALOG_MANIFEST:name="marketplace_catalog_manifest.json";break;
 case DW_MARKETPLACE_SKUS:name="marketplace_skus.bin";break;
 case DW_MARKETPLACE_PRODUCTS:name="marketplace_products.bin";break;
 default:return {};
 }
 std::ifstream file{"src/client/resources/dw/"+name,std::ios::binary};return {std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
} }

namespace demonware::hq_rewards { std::vector<achievement_record> initial_records() { return {}; } }
namespace demonware::achievement_response { std::optional<std::string> make_empty_scheduled_user_achievements_response(std::string_view) { return {}; } }
int main()
{
 using namespace demonware;
 profile = std::filesystem::temp_directory_path() / ("s2x-cr-catalog-test-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
 std::filesystem::create_directories(profile);
 const auto cleanup = utils::finally([&] { std::filesystem::remove_all(profile); });
 check(achievement_store::is_available());
 // The policy is shared by catalog emission and purchases; check bundle boundaries.
 unsigned contracts{}, bundles{};
 for (const auto& p : marketplace_cr::products) {
  check(p.id && p.price && p.items[0] && p.data.size() < 64 && p.text.size() < 135);
  check(marketplace_cr::find(p.id) == &p);
  if (p.bundle) ++bundles; else if (p.price == 2500) { ++contracts; }
 }
 check(contracts == 48 && bundles == 9 && marketplace_cwl::price == 1000);
 // Use the actual production backend predicate evaluator; mode/result and streak
 // event fields come from the captured script, not from the implementation.
 auto dom = order_progress::server_rule("daily_ch_dom_wins", 1, 5);
 check(dom.has_value());
 order_progress::event e{}; e.id=5; e.count=2; e.selectors={2,3}; e.values={2,1};
 check(order_progress::matches(*dom,e)); e.values[1]=2;check(!order_progress::matches(*dom,e));
 e.values={1,1};check(!order_progress::matches(*dom,e));e.count=1;check(!order_progress::matches(*dom,e));
 auto streak = order_progress::server_rule("daily_ch_destroy_scorestreaks",1,4);
 check(streak.has_value());e.count=1;e.selectors={2};e.values={3};check(order_progress::matches(*streak,e));
 e.values={2};check(!order_progress::matches(*streak,e));
 auto shotgun=order_progress::server_rule("daily_ch_winchesterloot2_1",1,2);
 check(shotgun.has_value());e.count=2;e.selectors={1,2};e.values={3,1};check(order_progress::matches(*shotgun,e));
 e.values={4,1};check(!order_progress::matches(*shotgun,e));
 // Exercise the production resource loader and scheduler over a whole rotation.
 std::set<std::string> seen;
 const auto base = static_cast<std::uint64_t>(std::time(nullptr));
 for (unsigned i=0;i<600;++i) {
  const auto plan=achievement_orders::make_schedule(base+i*86400);
  check(plan.valid);
  unsigned contracts_today{};
  for(const auto& offer:plan.orders) {
   seen.insert(offer.achievement.name);
   if(offer.achievement.kind==4 && !achievement_kind::moonraven(offer.achievement.name,4)) ++contracts_today;
  }
  check(contracts_today==9);
 }
 const auto rotations=achievement_orders::load_rotations();check(rotations.has_value());
 for(const auto& group:*rotations) for(const auto& set:group.sets) for(const auto& offer:set) check(seen.contains(offer.achievement.name));
 // Paid tokens retain their activation offer after it leaves the nine-card board.
 const auto first=achievement_orders::make_schedule(base);
 const auto retained=std::ranges::find_if(first.orders,[](const auto& o){return o.achievement.kind==4 && o.achievement.name=="contract_ch_mas38";});
 auto weapon=std::ranges::find_if(first.orders,[](const auto& o){return o.achievement.kind==4 && !achievement_kind::moonraven(o.achievement.name,4);});
 check(weapon!=first.orders.end());
 const auto later=achievement_orders::make_schedule(base+20*86400,weapon->achievement.name);
 check(later.valid && std::ranges::any_of(later.orders,[&](const auto& o){return o.achievement.name==weapon->achievement.name;}));
 const auto txn=[](std::string s){s.resize(24,'_');return s;};
 const auto& embedded=marketplace_catalog::get_embedded();
 if(!embedded.value) std::cerr<<embedded.error<<"\n";
 check(embedded.status==marketplace_catalog::load_status::ready && embedded.value);
 const auto& catalog=*embedded.value;
 const auto* front=catalog.find_skus(marketplace_sku::quartermaster_sku_type);
 check(front && front->size()>2);
 check(catalog.skus()[(*front)[0]].fields.sku_id==6 && catalog.skus()[(*front)[1]].fields.sku_id==2);
 check(marketplace_store::transact("fixture_wallet","fixture_wallet",[](auto& tx,auto& response){response="{}";return tx.add_currency(6,100000)==marketplace_store::edit_result::updated && tx.add_currency(2,10000)==marketplace_store::edit_result::updated;}).status==marketplace_store::transaction_status::committed);
 auto balance=[](unsigned id){const auto state=marketplace_store::get_snapshot();auto it=std::ranges::find(state.currencies,id,&marketplace_store::currency_record::currency_id);return it==state.currencies.end()?0u:it->value;};
 auto quantity=[](unsigned id){const auto state=marketplace_store::get_snapshot();auto it=std::ranges::find(state.inventory,id,&marketplace_store::inventory_record::item_id);return it==state.inventory.end()?0u:it->quantity;};
 const auto buy=[&](unsigned id,std::string transaction){auto it=std::ranges::find(catalog.skus(),id,[](const auto& sku){return sku.fields.sku_id;});check(it!=catalog.skus().end());auto product=catalog.find_product(it->fields.field_36);check(product);return marketplace_purchase::purchase({transaction,1,id},1,&it->fields,&product->fields);};
 // Native catalog loader, byte serialization, atomic purchasing and replay.
 for(const auto& policy:marketplace_cr::products) if(policy.bundle || policy.consumable) {
  const auto ac=balance(6),cp=balance(2),prior=quantity(policy.items[0]);
  const auto tx=txn("buy_"+std::to_string(policy.id));
  const auto result=buy(policy.id,tx);check(result.error==0 && !result.wire.empty());
  check(balance(6)==ac-policy.price && balance(2)==cp && quantity(policy.items[0])==prior+1);
  const auto repeated=buy(policy.id,tx);check(repeated.error==0 && quantity(policy.items[0])==prior+1 && balance(6)==ac-policy.price);
  if(policy.bundle) check(buy(policy.id,txn("other_"+std::to_string(policy.id))).error!=0);
  else {check(buy(policy.id,txn("other_"+std::to_string(policy.id))).error==0);check(quantity(policy.id)==prior+2 && balance(6)==ac-2*policy.price);}
 }
 // One current CR Zombies contract exercises purchase -> token consumption ->
 // timed progression -> claim, with the actual embedded offer and policy price.
 const auto current=achievement_orders::make_schedule(base);
 const marketplace_cr::product* contract_policy{};
 const achievement_store::order_offer* contract_offer{};
 for(const auto& offer:current.orders) if(offer.achievement.kind==11) {
  for(const auto& entry:marketplace_cr::products) if(!entry.bundle && !entry.consumable && entry.items[0]==offer.cost_item_id) {contract_offer=&offer;contract_policy=&entry;break;}
  if(contract_offer)break;
 }
 check(contract_offer && contract_policy);
 const auto ac_before_contract=balance(6);
 check(buy(contract_policy->id,txn("contract_purchase")).error==0);
 check(balance(6)==ac_before_contract-contract_policy->price && quantity(contract_offer->cost_item_id)==1);
 check(buy(contract_policy->id,txn("contract_purchase")).error==0);
 check(balance(6)==ac_before_contract-contract_policy->price && quantity(contract_offer->cost_item_id)==1);
 check(achievement_store::activate_order(contract_offer->achievement.name,11,1,txn("contract_activation"),*contract_offer,base)==achievement_store::activation_result::success);
 check(quantity(contract_offer->cost_item_id)==0);
 check(achievement_store::activate_order(contract_offer->achievement.name,11,1,txn("contract_activation"),*contract_offer,base)==achievement_store::activation_result::success);
 check(quantity(contract_offer->cost_item_id)==0);
 check(achievement_store::mutate(contract_offer->achievement.name,[](auto& record){record.progress=static_cast<std::uint16_t>(record.progress_target-1);return true;})==achievement_store::mutation_result::updated);
 const order_progress::target ct{contract_offer->achievement.name,11,base,contract_offer->achievement.progress_target};
 check(order_progress::settle({ct},base+1,{{ct,*contract_offer->achievement.usage_time_remaining-1}})==achievement_store::mutation_result::updated);
 unsigned contract_grants{};
 const auto claim_contract=[&](auto&,const auto& record,bool grant,const auto* bonus,auto& response){response="{}";check(record.success_rewards==contract_offer->achievement.success_rewards && !bonus);contract_grants+=grant;return true;};
 check(achievement_store::claim_order(ct.name,1,txn("contract_claim"),base+2,claim_contract).status==marketplace_store::transaction_status::committed);
 check(achievement_store::claim_order(ct.name,1,txn("contract_claim"),base+2,claim_contract).status==marketplace_store::transaction_status::replayed);
 check(contract_grants==1);
 const auto before_cp=balance(2),before_ac=balance(6);
 check(buy(0x40000006,txn("captured_CP_rare")).error==0);
 check(balance(2)<before_cp && balance(6)==before_ac);

 unsigned bonuses{}, payouts{};
 const auto timestamp=static_cast<std::uint64_t>(std::time(nullptr));
 const auto reward = [&](marketplace_store::transaction& tx,const achievement_record&,bool grant,const achievement_record* bonus,std::string& response) {
  response="{}";if (grant) {++payouts;check(tx.add_currency(6,1)==marketplace_store::edit_result::updated);}
  if (bonus) {++bonuses;check(bonus->name.starts_with("zm_above_beyond_"));check(bonus->success_rewards.find("\"id\":6")!=std::string::npos);}
  return true;
 };
 for (int kind : {8,9}) {
  const unsigned count=kind==8?6:3;
  for(unsigned i=0;i<count;++i) {
   auto name="test_zm_"+std::to_string(kind)+"_"+std::to_string(i);
   achievement_store::order_offer offer{};offer.achievement.name=name;offer.achievement.kind=kind;
   offer.achievement.progress_target=1;offer.achievement.requires_claim=true;
   offer.activation_limit=3;offer.period_start=timestamp-1;offer.next_period_start=timestamp+86400;
   check(achievement_store::activate_order(name,kind,1,txn("activate_"+name),offer,timestamp)==achievement_store::activation_result::success);
   const order_progress::target target{name,kind,timestamp,1};
   check(order_progress::settle({target},timestamp+1)==achievement_store::mutation_result::updated);
   // A failed sixth/third claim must roll back both the card and bonus counter.
   if(i+1==count) {
    auto failed=achievement_store::claim_order(name,1,txn("fail_"+name),timestamp+2,[](auto&,auto&,bool,auto*,auto&){return false;});
    check(failed.status==marketplace_store::transaction_status::rejected);
   }
   auto claimed=achievement_store::claim_order(name,1,txn("claim_"+name),timestamp+2,reward);
   check(claimed.status==marketplace_store::transaction_status::committed);
   const auto paid=payouts, awarded=bonuses;
   check(achievement_store::claim_order(name,1,txn("claim_"+name),timestamp+2,reward).status==marketplace_store::transaction_status::replayed);
   check(payouts==paid && bonuses==awarded);
   check(achievement_store::claim_order(name,1,txn("retry_"+name),timestamp+3,reward).status==marketplace_store::transaction_status::committed);
   check(payouts==paid && bonuses==awarded);
  }
 }
 check(payouts==9 && bonuses==2);
 const auto all=achievement_store::get_all();
 for(auto name:{"zm_above_beyond_daily","zm_above_beyond_weekly"}) {
  auto it=std::ranges::find(all,name,&achievement_record::name);check(it!=all.end());check(it->progress==0 && it->fulfilled_times==1);
 }
 for(auto name:{"above_beyond_daily","above_beyond_weekly"}) {
  auto it=std::ranges::find(all,name,&achievement_record::name);check(it!=all.end());check(it->progress==0 && it->fulfilled_times==0);
 }
 std::cout<<"PASS: production catalog, AC/CP purchases, replay, timed contract purchase/activation/claim,600-day schedules,predicates,ZM bonus rollback and MP isolation\n";
}
