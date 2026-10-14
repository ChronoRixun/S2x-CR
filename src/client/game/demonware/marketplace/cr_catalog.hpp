#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace demonware::marketplace_cr
{
 struct product
 {
  std::uint32_t id, price;
  std::string_view data, text;
  std::array<std::uint32_t, 10> items;
  bool bundle;
  bool consumable{};
 };
 // CR local policy; Rare weapon GUIDs and cost tokens use captured retail tables.
 inline constexpr product products[]{
  {0x0800F021, 100, "t:CONTRACT;c:162;C:162;i:s2_challenge_contracts", "TDM Headshots Contract;Timed objective", {0x5000019}, false},
  {0x0800F022, 350, "t:CONTRACT;c:561;C:561;i:s2_challenge_contracts", "SMG Kill Contract;Timed objective", {0x500006c}, false},
  {0x0800F023, 450, "t:CONTRACT;c:146;C:146;i:s2_challenge_contracts", "TDM Kill Contract;Timed objective", {0x5000009}, false},
  {0x0800F025, 350, "t:CONTRACT;c:149;C:149;i:s2_challenge_contracts", "Kill Contract;Timed objective", {0x500000c}, false},
  {0x0800F026, 100, "t:CONTRACT;c:153;C:153;i:s2_challenge_contracts", "Domination Kill Contract;Timed objective", {0x5000010}, false},
  {0x0800F027, 350, "t:CONTRACT;c:164;C:164;i:s2_challenge_contracts", "Headshots Contract;Timed objective", {0x500001b}, false},
  {0x0800F028, 100, "t:CONTRACT;c:204;C:204;i:s2_challenge_contracts", "LMG Kill Contract;Timed objective", {0x5000043}, false},
  {0x0800F029, 450, "t:CONTRACT;c:562;C:562;i:s2_challenge_contracts", "LMG Supply Contract;Timed objective", {0x500006d}, false},
  {0x0800F031, 100, "t:CONTRACT;c:1074;C:1074;i:s2_challenge_contracts_zm", "Zombie Hunter;Timed objective", {0x5000054}, false},
  {0x0800F032, 250, "t:CONTRACT;c:1075;C:1075;i:s2_challenge_contracts_zm", "Zombie Slayer;Timed objective", {0x5000055}, false},
  {0x0800F033, 450, "t:CONTRACT;c:1080;C:1080;i:s2_challenge_contracts_zm", "Zombie Exterminator;Timed objective", {0x500005A}, false},
  {0x0800F034, 150, "t:CONTRACT;c:1077;C:1077;i:s2_challenge_contracts_zm", "Airborne Blades;Timed objective", {0x5000057}, false},
  {0x0800F035, 200, "t:CONTRACT;c:1081;C:1081;i:s2_challenge_contracts_zm", "Bestiary Bounty;Timed objective", {0x500005B}, false},
  {0x0800F036, 300, "t:CONTRACT;c:1082;C:1082;i:s2_challenge_contracts_zm", "Ripsaw Reaper;Timed objective", {0x500005C}, false},
  {0x0800F037, 350, "t:CONTRACT;c:1084;C:1084;i:s2_challenge_contracts_zm", "Freefire Frenzy;Timed objective", {0x500005E}, false},
  {0x0800F038, 250, "t:CONTRACT;c:1088;C:1088;i:s2_challenge_contracts_zm", "Treasure Hunter;Timed objective", {0x5000062}, false},

  {2, 1000, "t:MP", "LUA_MENU_RARE_SUPPLY_DROP;3 random items", {2}, false, true},
  {6, 1000, "t:ZM", "LUA_MENU_RARE_ZOMBIE_SUPPLY_DROP;2 items + 3 consumables", {6}, false, true},
  {0x0800F040, 2500, "t:CONTRACT;c:566;C:566;i:s2_challenge_contracts", "AEC_CONTRACT_SPECIAL_SMG_TITLE;Timed objective", {0x5000071}, false},
  {0x0800F041, 2500, "t:CONTRACT;c:567;C:567;i:s2_challenge_contracts", "AEC_CONTRACT_SPECIAL_LMG_TITLE;Timed objective", {0x5000072}, false},
  {0x0800F042, 2500, "t:CONTRACT;c:573;C:573;i:s2_challenge_contracts", "AEC_CONTRACT_STEN_TITLE;Timed objective", {0x5000073}, false},
  {0x0800F043, 2500, "t:CONTRACT;c:574;C:574;i:s2_challenge_contracts", "AEC_CONTRACT_GEWEHR_TITLE;Timed objective", {0x5000074}, false},
  {0x0800F044, 2500, "t:CONTRACT;c:575;C:575;i:s2_challenge_contracts", "AEC_CONTRACT_GPMG_TITLE;Timed objective", {0x5000075}, false},
  {0x0800F045, 2500, "t:CONTRACT;c:576;C:576;i:s2_challenge_contracts", "AEC_CONTRACT_ICEPICK_TITLE;Timed objective", {0x5000076}, false},
  {0x0800F046, 2500, "t:CONTRACT;c:577;C:577;i:s2_challenge_contracts", "AEC_CONTRACT_TRENCHKNIFE_TITLE;Timed objective", {0x5000077}, false},
  {0x0800F047, 2500, "t:CONTRACT;c:578;C:578;i:s2_challenge_contracts", "AEC_CONTRACT_VOLK_TITLE;Timed objective", {0x5000078}, false},
  {0x0800F048, 2500, "t:CONTRACT;c:579;C:579;i:s2_challenge_contracts", "AEC_CONTRACT_ORSO_TITLE;Timed objective", {0x5000079}, false},
  {0x0800F049, 2500, "t:CONTRACT;c:580;C:580;i:s2_challenge_contracts", "AEC_CONTRACT_COMBATKNIFE_TITLE;Timed objective", {0x500007A}, false},
  {0x0800F04A, 2500, "t:CONTRACT;c:581;C:581;i:s2_challenge_contracts", "AEC_CONTRACT_ENFIELD_TITLE;Timed objective", {0x500007B}, false},
  {0x0800F04B, 2500, "t:CONTRACT;c:582;C:582;i:s2_challenge_contracts", "AEC_CONTRACT_REVOLVER_TITLE;Timed objective", {0x500007C}, false},
  {0x0800F024, 2500, "t:CONTRACT;c:3048;C:3048;i:s2_challenge_contracts", "LAD Machine Gun Contract;Timed objective", {0x50000B9}, false},
  {0x0800F04C, 2500, "t:CONTRACT;c:614;C:614;i:s2_challenge_contracts", "AEC_CONTRACT_BASEBALLBAT_TITLE;Timed objective", {0x5000083}, false},
  {0x0800F04D, 2500, "t:CONTRACT;c:615;C:615;i:s2_challenge_contracts", "AEC_CONTRACT_TYPE_5_TITLE;Timed objective", {0x5000084}, false},
  {0x0800F04E, 2500, "t:CONTRACT;c:616;C:616;i:s2_challenge_contracts", "AEC_CONTRACT_STERLING_TITLE;Timed objective", {0x5000085}, false},
  {0x0800F04F, 2500, "t:CONTRACT;c:617;C:617;i:s2_challenge_contracts", "AEC_CONTRACT_ARISAKA_TITLE;Timed objective", {0x5000086}, false},
  {0x0800F050, 2500, "t:CONTRACT;c:618;C:618;i:s2_challenge_contracts", "AEC_CONTRACT_BURSTRIFLE_TITLE;Timed objective", {0x5000087}, false},
  {0x0800F051, 2500, "t:CONTRACT;c:619;C:619;i:s2_challenge_contracts", "AEC_CONTRACT_AUTOM2_TITLE;Timed objective", {0x5000088}, false},
  {0x0800F052, 2500, "t:CONTRACT;c:746;C:746;i:s2_challenge_contracts", "AEC_CONTRACT_CLAYMORE_TITLE;Timed objective", {0x5000094}, false},
  {0x0800F053, 2500, "t:CONTRACT;c:747;C:747;i:s2_challenge_contracts", "AEC_CONTRACT_LEVER_ACTION_TITLE;Timed objective", {0x5000095}, false},
  {0x0800F054, 2500, "t:CONTRACT;c:748;C:748;i:s2_challenge_contracts", "AEC_CONTRACT_FIRE_AXE_TITLE;Timed objective", {0x5000096}, false},
  {0x0800F055, 2500, "t:CONTRACT;c:749;C:749;i:s2_challenge_contracts", "AEC_CONTRACT_M1919_TITLE;Timed objective", {0x5000097}, false},
  {0x0800F056, 2500, "t:CONTRACT;c:750;C:750;i:s2_challenge_contracts", "AEC_CONTRACT_NAMBU_TITLE;Timed objective", {0x5000098}, false},
  {0x0800F057, 2500, "t:CONTRACT;c:751;C:751;i:s2_challenge_contracts", "AEC_CONTRACT_BLUNDERBUSS_TITLE;Timed objective", {0x5000099}, false},
  {0x0800F058, 2500, "t:CONTRACT;c:752;C:752;i:s2_challenge_contracts", "AEC_CONTRACT_PTRS_TITLE;Timed objective", {0x500009A}, false},
  {0x0800F059, 2500, "t:CONTRACT;c:847;C:847;i:s2_challenge_contracts", "AEC_CONTRACT_AVS_TITLE;Timed objective", {0x50000A7}, false},
  {0x0800F05A, 2500, "t:CONTRACT;c:849;C:849;i:s2_challenge_contracts", "AEC_CONTRACT_DELISLE_TITLE;Timed objective", {0x50000A9}, false},
  {0x0800F05B, 2500, "t:CONTRACT;c:850;C:850;i:s2_challenge_contracts", "AEC_CONTRACT_RIBEYROLLS_TITLE;Timed objective", {0x50000AA}, false},
  {0x0800F05C, 2500, "t:CONTRACT;c:851;C:851;i:s2_challenge_contracts", "AEC_CONTRACT_3LINE_TITLE;Timed objective", {0x50000AB}, false},
  {0x0800F05D, 2500, "t:CONTRACT;c:879;C:879;i:s2_challenge_contracts", "AEC_CONTRACT_VMG_TITLE;Timed objective", {0x50000AE}, false},
  {0x0800F05E, 2500, "t:CONTRACT;c:880;C:880;i:s2_challenge_contracts", "AEC_CONTRACT_TOKYO_TITLE;Timed objective", {0x50000AF}, false},
  {0x0800F05F, 2500, "t:CONTRACT;c:935;C:935;i:s2_challenge_contracts", "AEC_CONTRACT_CROSSBOW_TITLE;Timed objective", {0x50000B0}, false},
  {0x0800F060, 2500, "t:CONTRACT;c:936;C:936;i:s2_challenge_contracts", "AEC_CONTRACT_EMP_TITLE;Timed objective", {0x50000B1}, false},
  {0x0800F061, 2500, "t:CONTRACT;c:937;C:937;i:s2_challenge_contracts", "AEC_CONTRACT_CHARLTON_TITLE;Timed objective", {0x50000B2}, false},
  {0x0800F062, 2500, "t:CONTRACT;c:939;C:939;i:s2_challenge_contracts", "AEC_CONTRACT_SLEDGE_TITLE;Timed objective", {0x50000B4}, false},
  {0x0800F063, 2500, "t:CONTRACT;c:3045;C:3045;i:s2_challenge_contracts", "AEC_CONTRACT_KG_TITLE;Timed objective", {0x50000B6}, false},
  {0x0800F064, 2500, "t:CONTRACT;c:3046;C:3046;i:s2_challenge_contracts", "AEC_CONTRACT_GDB_TITLE;Timed objective", {0x50000B7}, false},
  {0x0800F065, 2500, "t:CONTRACT;c:3047;C:3047;i:s2_challenge_contracts", "AEC_CONTRACT_WIMMER_TITLE;Timed objective", {0x50000B8}, false},
  {0x0800F066, 2500, "t:CONTRACT;c:3049;C:3049;i:s2_challenge_contracts", "AEC_CONTRACT_CHATELLERAULT_TITLE;Timed objective", {0x50000BA}, false},
  {0x0800F067, 2500, "t:CONTRACT;c:3050;C:3050;i:s2_challenge_contracts", "AEC_CONTRACT_AUSTEN_TITLE;Timed objective", {0x50000BB}, false},
  {0x0800F068, 2500, "t:CONTRACT;c:3051;C:3051;i:s2_challenge_contracts", "AEC_CONTRACT_BECHOWIEC_TITLE;Timed objective", {0x50000BC}, false},
  {0x0800F069, 2500, "t:CONTRACT;c:3052;C:3052;i:s2_challenge_contracts", "AEC_CONTRACT_BLYSKAWICA_TITLE;Timed objective", {0x50000BD}, false},
  {0x0800F06A, 2500, "t:CONTRACT;c:3053;C:3053;i:s2_challenge_contracts", "AEC_CONTRACT_M2_TITLE;Timed objective", {0x50000BE}, false},
  {0x0800F06B, 2500, "t:CONTRACT;c:3054;C:3054;i:s2_challenge_contracts", "AEC_CONTRACT_ERMA_TITLE;Timed objective", {0x50000BF}, false},
  {0x0800F06C, 2500, "t:CONTRACT;c:3055;C:3055;i:s2_challenge_contracts", "AEC_CONTRACT_M36_TITLE;Timed objective", {0x50000C0}, false},
  {0x0800F06D, 2500, "t:CONTRACT;c:3056;C:3056;i:s2_challenge_contracts", "AEC_CONTRACT_WZ_TITLE;Timed objective", {0x50000C1}, false},
  {0x0800F06E, 2500, "t:CONTRACT;c:3057;C:3057;i:s2_challenge_contracts", "AEC_CONTRACT_DAGGER_TITLE;Timed objective", {0x50000C2}, false},
  {0x2000004, 1000, "t:S2X_CODE;l:0x2000004|1;i:emblem_incentive_code", "C.O.D.E. Pack;3 cosmetics", {0x2000004, 0x800093, 0x2400225, 0x6600100}, true},
  {0x2000003, 1000, "t:S2X_BETA;l:0x2000003|1;i:emblem_incentive_beta", "Beta and Ambassador Pack;7 cosmetics", {0x2000003, 0x2400222, 0x6632101, 0x2000005, 0x2400267, 0x2400268, 0x2400269}, true},
  {0x700013f, 1000, "t:S2X_ZOMBIES;l:0x700013f|1;i:charm_ray_gun", "Zombies Pack;9 cosmetics", {0x700013f, 0x7040001, 0x7040002, 0x7040003, 0x7040015, 0x7040016, 0x7040017, 0x7040020, 0x7040022}, true},
  {0x6003001, 16250, "t:S2X_PREORDER;l:0x6003001|1;i:us_infantry_skin1", "Divisions Pre-order Pack;5 cosmetics", {0x6003001, 0x6003003, 0x6003009, 0x600300d, 0x6003010}, true},
  {0x7000096, 1000, "t:S2X_VIPER;l:0x7000096|1;i:charm_fdr_dog", "C.O.D.E. Viper Pack;9 cosmetics", {0x7000096, 0x80013a, 0x7018017, 0x7019017, 0x701a017, 0x701b017, 0x701c017, 0x701d017, 0x701e017, 0x701f017}, true},
  {0x600002f, 3750, "t:S2X_ENDOWMENT;l:0x600002f|1;i:monty_r1", "C.O.D.E. Endowment Pack;5 cosmetics", {0x600002f, 0x80013a, 0x2000108, 0x24003f9, 0x2000107, 0x24003f8}, true},
  {0x66000e5, 1000, "t:S2X_PARTNER;l:0x66000e5|1;i:special_helmet_pathfinder", "Partner Pack;7 cosmetics", {0x66000e5, 0x2400223, 0x2400226, 0x2400224, 0x240026a, 0x240026b, 0x24003cc}, true},
  {0x6000038, 3250, "t:S2X_TWITCH;l:0x6000038|1;i:twitch_r1", "Twitch Uniform;1 cosmetic", {0x6000038}, true},
  {0x6023003, 6500, "t:S2X_ARMYMEN;l:0x6023003|1;i:us_paratrooper_skin3", "Army Men Pack;2 cosmetics", {0x6023003, 0x6023022}, true},
 };
 inline const product* find(const std::uint32_t id)
 {
  for (const auto& entry : products) if (entry.id == id) return &entry;
  return nullptr;
 }
}
