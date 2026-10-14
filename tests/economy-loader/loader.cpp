#include <std_include.hpp>
#include "game/demonware/marketplace/store.hpp"
#include "game/demonware/achievement/store.hpp"
#include <iostream>
#include <cassert>
namespace { std::filesystem::path profile; }
// Native catalog seed access is excluded; the fixture itself supplies all records.
namespace demonware::hq_rewards { std::vector<achievement_record> initial_records(){return {};} }
namespace demonware::achievement_orders { uint64_t moonraven_expiration(uint64_t timestamp){constexpr uint64_t offset=17*60*60,period=30*24*60*60;return timestamp<offset || timestamp>UINT64_MAX-period?0:offset+((timestamp-offset)/period+1)*period;} }
namespace player_profile { const std::filesystem::path& user_directory(){return profile;} }
int wmain(int argc,wchar_t**argv){
 if(argc!=2)return 3;profile=argv[1];
 if(!std::filesystem::is_regular_file(profile / "marketplace.json"))return 4;
 const auto state=demonware::marketplace_store::get_snapshot();
 if(state.status!=demonware::marketplace_store::store_status::ready){std::cout<<"REJECT native store status "<<static_cast<int>(state.status)<<"\n";return 2;}
 if(!demonware::achievement_store::is_available()){std::cout<<"REJECT native achievement state\n";return 2;}
 const auto achievements=demonware::achievement_store::get_all();
 for(const auto& name:{"above_beyond_daily","above_beyond_weekly","zm_above_beyond_daily","zm_above_beyond_weekly"}){
  const auto found=std::ranges::find(achievements,std::string{name},&demonware::achievement_record::name);
  assert(found!=achievements.end());assert(found->success_rewards!="[]");assert(found->progress_target==((std::string{name}.find("weekly")!=std::string::npos)?3u:6u));
 }
 std::cout<<"PASS canonical MP/ZM meta descriptors and six/three targets\n";

 const auto settled=demonware::marketplace_store::find_transaction("prestige:mp:1");
 if(settled){bool called=false;const auto replay=demonware::marketplace_store::transact(settled->client_tx,settled->request_fingerprint,[&](auto&,auto&){called=true;return false;});assert(!called&&replay.status==demonware::marketplace_store::transaction_status::replayed);const auto conflict=demonware::marketplace_store::transact(settled->client_tx,"different-request",[&](auto&,auto&){called=true;return false;});assert(!called&&conflict.status==demonware::marketplace_store::transaction_status::client_tx_conflict);std::cout<<"PASS native migrated receipt replay/conflict isolation\n";}
 std::cout<<"ACCEPT native loader: currencies="<<state.currencies.size()<<" inventory="<<state.inventory.size()<<" achievements="<<achievements.size()<<"\n";
 return 0;
}
