#pragma once

#include "hq_economy.hpp"
#include "byte_buffer.hpp"
#include <array>

namespace demonware::hq_marketplace
{
	struct inventory_request
	{
		std::uint32_t page{};
		std::uint32_t limit{};
	};
	struct sku_request : inventory_request
	{
		std::vector<std::uint32_t> ids;
		std::vector<unsigned char> types;
	};
	struct sku
	{
		std::uint32_t id{}, price{};
		unsigned char type{100};
		// SKU data string, returned by Engine.Inventory_GetSKUInfoSKUData and parsed by
		// QuarterMasterUtils.FindSkuDataByType as "key:value;key:value" (LUI.SingleSplit on
		// ';' then ':'). Keys are single letters, QuarterMasterUtils.SKUDataKeys: t = tag,
		// i = image override, f = flags, c/C = contract id, l = limiter "<guid>|<max>",
		// u = unlock guid, s = special icons, e = experiment cohort. Native capacity 64
		// bytes including the terminator; the collection catalog carries none of them.
		const char* data{""};
		// Promotional text, split on ';' by QuarterMasterUtils.ProcessSkuInfo into the
		// vendor tile's name and description (both Localize()d). The Demonware blob holds
		// 135 bytes; the native SKU cache slot only 64, at +0x25C (Inventory_GetSKUInfo,
		// binding 0x11FF90, reads promotionalText there and skuData at +0x29C).
		const char* promotional_text{""};
		// A CWL pack grants five real cosmetic GUIDs from the decompiled details menu.
		std::array<std::uint32_t, 5> items{};
		const char* contract{""};
		bool consumable{};
		// Currency id of the single price record the client reads: task 111 catalog record
		// price +0x20 (hq_vendor::catalog_result) and native SKU cache slot +0x24C
		// (hq_native::sku_lookup, the field Inventory_GetSKUInfo returns as
		// prices[1].currency). Everything this store sells is priced in Armory Credits and
		// hq_marketplace::purchase debits nothing else, so the two writers must never fall
		// back to a hard-coded constant of their own.
		std::uint8_t currency{hq_economy::armory_credits};
	};
	// The Quartermaster front page cannot be built without these two.
	// QuarterMasterUtils.GetAvailableSkuIDList (decompiled:
	// build/research/luafiles/dec/ui_utility_mp_quartermaster_utils.dec.lua) appends, after
	// the optional scheduled specials, FindSKUIDByType(skus, SupplyDropTypeTag.ASD_MP = "MP")
	// and FindSKUIDByType(skus, ASD_ZOMBIE = "ZM") - the tag is the "t" entry of the SKU data
	// string. storeSKUInfo then walks that list through a bare `assert(entry.id)`, so a
	// catalog with no "MP" and no "ZM" SKU raises "LUI.MenuBuilder.buildItems(): assertion
	// failed" in the menu's PreLoadFunc and the menu never appears. The GUIDs are the supply
	// drop item ids of mp/supplyDropTypes.csv column f5: sd_mp_rare = 2, sd_zombie_rare = 6.
	// They lead the catalog because the native SKU cache only holds 400 of its entries and
	// FindSKUIDByType returns the first match.
	inline constexpr sku vendor_skus[]
	{
		// Local AC prices and bundle policy; tags/images and cosmetic GUIDs are shipped LUI data.
		{2, 1000, 100, "t:MP", "LUA_MENU_RARE_SUPPLY_DROP;3 random items", {}, "", true},
		{6, 1000, 100, "t:ZM", "LUA_MENU_RARE_ZOMBIE_SUPPLY_DROP;2 items + 3 consumables", {}, "", true},
		// SKUType.Quartermaster (100), not SKUType.Contracts (201). The shipped LUI defines
		// SKUType.Contracts but never fetches it: DwDataUtils._fetchSKUData only calls
		// Engine.Inventory_FetchAllSKUs(controller, SKUType.Quartermaster) and
		// DwDataUtils._updateSKUData only calls Engine.Inventory_GetAllSKUIDs(SKUType.Quartermaster),
		// and every contract lookup - QuarterMasterUtils.GetContractCurrencies (key "c"), the
		// sku_details "VIEW CONTRACT" option (key "C") - walks that one cached list. A record
		// typed 201 in the native SKU cache (+0x04, written by hq_native::sku_lookup) is not a
		// Quartermaster SKU. The contract id lives in the SKU data string, not in the type.
		// Zombies retail cost tokens; native achievement kind 11. Keep these in the
		// same Quartermaster cache (type 100) used by the shared contract helpers.
		{0x0800F031, 100, 100, "t:CONTRACT;c:1074;C:1074;i:s2_challenge_contracts_zm", "Zombie Hunter;Timed objective", {0x5000054}, "contract_zm_ch_kills_1"},
		{0x0800F032, 250, 100, "t:CONTRACT;c:1075;C:1075;i:s2_challenge_contracts_zm", "Zombie Slayer;Timed objective", {0x5000055}, "contract_zm_ch_kills_2"},
		{0x0800F033, 450, 100, "t:CONTRACT;c:1080;C:1080;i:s2_challenge_contracts_zm", "Zombie Exterminator;Timed objective", {0x500005A}, "contract_zm_ch_kills_3"},
		{0x0800F034, 150, 100, "t:CONTRACT;c:1077;C:1077;i:s2_challenge_contracts_zm", "Airborne Blades;Timed objective", {0x5000057}, "contract_zm_ch_jumping_knifes_1"},
		{0x0800F035, 200, 100, "t:CONTRACT;c:1081;C:1081;i:s2_challenge_contracts_zm", "Bestiary Bounty;Timed objective", {0x500005B}, "contract_zm_ch_unique_kills_fr_1"},
		{0x0800F036, 300, 100, "t:CONTRACT;c:1082;C:1082;i:s2_challenge_contracts_zm", "Ripsaw Reaper;Timed objective", {0x500005C}, "contract_zm_ch_beheadings_1"},
		{0x0800F037, 350, 100, "t:CONTRACT;c:1084;C:1084;i:s2_challenge_contracts_zm", "Freefire Frenzy;Timed objective", {0x500005E}, "contract_zm_kills_freefire_1"},
		{0x0800F038, 250, 100, "t:CONTRACT;c:1088;C:1088;i:s2_challenge_contracts_zm", "Treasure Hunter;Timed objective", {0x5000062}, "contract_zm_treasure_1"},
		{0x0800F021, 100, 100, "t:CONTRACT;c:162;C:162;i:s2_challenge_contracts", "TDM Headshots Contract;Timed objective", {0x5000019}, "contract_4_headshots_tdm"},
		{0x0800F022, 350, 100, "t:CONTRACT;c:561;C:561;i:s2_challenge_contracts", "SMG Kill Contract;Timed objective", {0x500006c}, "contract_50_kills_smg"},
		{0x0800F023, 450, 100, "t:CONTRACT;c:146;C:146;i:s2_challenge_contracts", "TDM Kill Contract;Timed objective", {0x5000009}, "contract_55_kills_tdm"},
		{0x0800F024, 2500, 100, "t:CONTRACT;c:3048;C:3048;i:s2_challenge_contracts", "LAD Machine Gun Contract;Timed objective", {0x50000B9}, "contract_ch_lad"},
		{0x0800F025, 350, 100, "t:CONTRACT;c:149;C:149;i:s2_challenge_contracts", "Kill Contract;Timed objective", {0x500000c}, "contract_45_kills"},
		{0x0800F026, 100, 100, "t:CONTRACT;c:153;C:153;i:s2_challenge_contracts", "Domination Kill Contract;Timed objective", {0x5000010}, "contract_25_kills_dom"},
		{0x0800F027, 350, 100, "t:CONTRACT;c:164;C:164;i:s2_challenge_contracts", "Headshots Contract;Timed objective", {0x500001b}, "contract_9_headshots"},
		{0x0800F028, 100, 100, "t:CONTRACT;c:204;C:204;i:s2_challenge_contracts", "LMG Kill Contract;Timed objective", {0x5000043}, "contract_25_kills_lmg"},
		{0x0800F029, 450, 100, "t:CONTRACT;c:562;C:562;i:s2_challenge_contracts", "LMG Supply Contract;Timed objective", {0x500006d}, "contract_50_kills_lmg"},
		{0x0800F040, 2500, 100, "t:CONTRACT;c:566;C:566;i:s2_challenge_contracts", "AEC_CONTRACT_SPECIAL_SMG_TITLE;Timed objective", {0x5000071}, "contract_ch_mas38"},
		{0x0800F041, 2500, 100, "t:CONTRACT;c:567;C:567;i:s2_challenge_contracts", "AEC_CONTRACT_SPECIAL_LMG_TITLE;Timed objective", {0x5000072}, "contract_ch_mg81"},
		{0x0800F042, 2500, 100, "t:CONTRACT;c:573;C:573;i:s2_challenge_contracts", "AEC_CONTRACT_STEN_TITLE;Timed objective", {0x5000073}, "contract_ch_sten"},
		{0x0800F043, 2500, 100, "t:CONTRACT;c:574;C:574;i:s2_challenge_contracts", "AEC_CONTRACT_GEWEHR_TITLE;Timed objective", {0x5000074}, "contract_ch_gewehr"},
		{0x0800F044, 2500, 100, "t:CONTRACT;c:575;C:575;i:s2_challenge_contracts", "AEC_CONTRACT_GPMG_TITLE;Timed objective", {0x5000075}, "contract_ch_gpmg"},
		{0x0800F045, 2500, 100, "t:CONTRACT;c:576;C:576;i:s2_challenge_contracts", "AEC_CONTRACT_ICEPICK_TITLE;Timed objective", {0x5000076}, "contract_ch_icepick"},
		{0x0800F046, 2500, 100, "t:CONTRACT;c:577;C:577;i:s2_challenge_contracts", "AEC_CONTRACT_TRENCHKNIFE_TITLE;Timed objective", {0x5000077}, "contract_ch_trenchknife"},
		{0x0800F047, 2500, 100, "t:CONTRACT;c:578;C:578;i:s2_challenge_contracts", "AEC_CONTRACT_VOLK_TITLE;Timed objective", {0x5000078}, "contract_ch_volk"},
		{0x0800F048, 2500, 100, "t:CONTRACT;c:579;C:579;i:s2_challenge_contracts", "AEC_CONTRACT_ORSO_TITLE;Timed objective", {0x5000079}, "contract_ch_orso"},
		{0x0800F049, 2500, 100, "t:CONTRACT;c:580;C:580;i:s2_challenge_contracts", "AEC_CONTRACT_COMBATKNIFE_TITLE;Timed objective", {0x500007A}, "contract_ch_combatknife"},
		{0x0800F04A, 2500, 100, "t:CONTRACT;c:581;C:581;i:s2_challenge_contracts", "AEC_CONTRACT_ENFIELD_TITLE;Timed objective", {0x500007B}, "contract_ch_enfield"},
		{0x0800F04B, 2500, 100, "t:CONTRACT;c:582;C:582;i:s2_challenge_contracts", "AEC_CONTRACT_REVOLVER_TITLE;Timed objective", {0x500007C}, "contract_ch_revolver"},
		{0x0800F04C, 2500, 100, "t:CONTRACT;c:614;C:614;i:s2_challenge_contracts", "AEC_CONTRACT_BASEBALLBAT_TITLE;Timed objective", {0x5000083}, "contract_ch_baseballbat"},
		{0x0800F04D, 2500, 100, "t:CONTRACT;c:615;C:615;i:s2_challenge_contracts", "AEC_CONTRACT_TYPE_5_TITLE;Timed objective", {0x5000084}, "contract_ch_type5"},
		{0x0800F04E, 2500, 100, "t:CONTRACT;c:616;C:616;i:s2_challenge_contracts", "AEC_CONTRACT_STERLING_TITLE;Timed objective", {0x5000085}, "contract_ch_sterling"},
		{0x0800F04F, 2500, 100, "t:CONTRACT;c:617;C:617;i:s2_challenge_contracts", "AEC_CONTRACT_ARISAKA_TITLE;Timed objective", {0x5000086}, "contract_ch_arisaka"},
		{0x0800F050, 2500, 100, "t:CONTRACT;c:618;C:618;i:s2_challenge_contracts", "AEC_CONTRACT_BURSTRIFLE_TITLE;Timed objective", {0x5000087}, "contract_ch_burstrifle"},
		{0x0800F051, 2500, 100, "t:CONTRACT;c:619;C:619;i:s2_challenge_contracts", "AEC_CONTRACT_AUTOM2_TITLE;Timed objective", {0x5000088}, "contract_ch_autom2"},
		{0x0800F052, 2500, 100, "t:CONTRACT;c:746;C:746;i:s2_challenge_contracts", "AEC_CONTRACT_CLAYMORE_TITLE;Timed objective", {0x5000094}, "contract_ch_claymore"},
		{0x0800F053, 2500, 100, "t:CONTRACT;c:747;C:747;i:s2_challenge_contracts", "AEC_CONTRACT_LEVER_ACTION_TITLE;Timed objective", {0x5000095}, "contract_ch_lever_action"},
		{0x0800F054, 2500, 100, "t:CONTRACT;c:748;C:748;i:s2_challenge_contracts", "AEC_CONTRACT_FIRE_AXE_TITLE;Timed objective", {0x5000096}, "contract_ch_fire_axe"},
		{0x0800F055, 2500, 100, "t:CONTRACT;c:749;C:749;i:s2_challenge_contracts", "AEC_CONTRACT_M1919_TITLE;Timed objective", {0x5000097}, "contract_ch_m1919"},
		{0x0800F056, 2500, 100, "t:CONTRACT;c:750;C:750;i:s2_challenge_contracts", "AEC_CONTRACT_NAMBU_TITLE;Timed objective", {0x5000098}, "contract_ch_nambu"},
		{0x0800F057, 2500, 100, "t:CONTRACT;c:751;C:751;i:s2_challenge_contracts", "AEC_CONTRACT_BLUNDERBUSS_TITLE;Timed objective", {0x5000099}, "contract_ch_blunderbuss"},
		{0x0800F058, 2500, 100, "t:CONTRACT;c:752;C:752;i:s2_challenge_contracts", "AEC_CONTRACT_PTRS_TITLE;Timed objective", {0x500009A}, "contract_ch_ptrs"},
		{0x0800F059, 2500, 100, "t:CONTRACT;c:847;C:847;i:s2_challenge_contracts", "AEC_CONTRACT_AVS_TITLE;Timed objective", {0x50000A7}, "contract_ch_avs"},
		{0x0800F05A, 2500, 100, "t:CONTRACT;c:849;C:849;i:s2_challenge_contracts", "AEC_CONTRACT_DELISLE_TITLE;Timed objective", {0x50000A9}, "contract_ch_delisle"},
		{0x0800F05B, 2500, 100, "t:CONTRACT;c:850;C:850;i:s2_challenge_contracts", "AEC_CONTRACT_RIBEYROLLS_TITLE;Timed objective", {0x50000AA}, "contract_ch_ribeyrolls"},
		{0x0800F05C, 2500, 100, "t:CONTRACT;c:851;C:851;i:s2_challenge_contracts", "AEC_CONTRACT_3LINE_TITLE;Timed objective", {0x50000AB}, "contract_ch_3line"},
		{0x0800F05D, 2500, 100, "t:CONTRACT;c:879;C:879;i:s2_challenge_contracts", "AEC_CONTRACT_VMG_TITLE;Timed objective", {0x50000AE}, "contract_ch_vmg"},
		{0x0800F05E, 2500, 100, "t:CONTRACT;c:880;C:880;i:s2_challenge_contracts", "AEC_CONTRACT_TOKYO_TITLE;Timed objective", {0x50000AF}, "contract_ch_tokyo"},
		{0x0800F05F, 2500, 100, "t:CONTRACT;c:935;C:935;i:s2_challenge_contracts", "AEC_CONTRACT_CROSSBOW_TITLE;Timed objective", {0x50000B0}, "contract_ch_crossbow"},
		{0x0800F060, 2500, 100, "t:CONTRACT;c:936;C:936;i:s2_challenge_contracts", "AEC_CONTRACT_EMP_TITLE;Timed objective", {0x50000B1}, "contract_ch_emp"},
		{0x0800F061, 2500, 100, "t:CONTRACT;c:937;C:937;i:s2_challenge_contracts", "AEC_CONTRACT_CHARLTON_TITLE;Timed objective", {0x50000B2}, "contract_ch_charlton"},
		{0x0800F062, 2500, 100, "t:CONTRACT;c:939;C:939;i:s2_challenge_contracts", "AEC_CONTRACT_SLEDGE_TITLE;Timed objective", {0x50000B4}, "contract_ch_sledge"},
		{0x0800F063, 2500, 100, "t:CONTRACT;c:3045;C:3045;i:s2_challenge_contracts", "AEC_CONTRACT_KG_TITLE;Timed objective", {0x50000B6}, "contract_ch_kg"},
		{0x0800F064, 2500, 100, "t:CONTRACT;c:3046;C:3046;i:s2_challenge_contracts", "AEC_CONTRACT_GDB_TITLE;Timed objective", {0x50000B7}, "contract_ch_gdb"},
		{0x0800F065, 2500, 100, "t:CONTRACT;c:3047;C:3047;i:s2_challenge_contracts", "AEC_CONTRACT_WIMMER_TITLE;Timed objective", {0x50000B8}, "contract_ch_wimmer"},
		{0x0800F066, 2500, 100, "t:CONTRACT;c:3049;C:3049;i:s2_challenge_contracts", "AEC_CONTRACT_CHATELLERAULT_TITLE;Timed objective", {0x50000BA}, "contract_ch_chatellerault"},
		{0x0800F067, 2500, 100, "t:CONTRACT;c:3050;C:3050;i:s2_challenge_contracts", "AEC_CONTRACT_AUSTEN_TITLE;Timed objective", {0x50000BB}, "contract_ch_austen"},
		{0x0800F068, 2500, 100, "t:CONTRACT;c:3051;C:3051;i:s2_challenge_contracts", "AEC_CONTRACT_BECHOWIEC_TITLE;Timed objective", {0x50000BC}, "contract_ch_bechowiec"},
		{0x0800F069, 2500, 100, "t:CONTRACT;c:3052;C:3052;i:s2_challenge_contracts", "AEC_CONTRACT_BLYSKAWICA_TITLE;Timed objective", {0x50000BD}, "contract_ch_blyskawica"},
		{0x0800F06A, 2500, 100, "t:CONTRACT;c:3053;C:3053;i:s2_challenge_contracts", "AEC_CONTRACT_M2_TITLE;Timed objective", {0x50000BE}, "contract_ch_m2"},
		{0x0800F06B, 2500, 100, "t:CONTRACT;c:3054;C:3054;i:s2_challenge_contracts", "AEC_CONTRACT_ERMA_TITLE;Timed objective", {0x50000BF}, "contract_ch_erma"},
		{0x0800F06C, 2500, 100, "t:CONTRACT;c:3055;C:3055;i:s2_challenge_contracts", "AEC_CONTRACT_M36_TITLE;Timed objective", {0x50000C0}, "contract_ch_m36"},
		{0x0800F06D, 2500, 100, "t:CONTRACT;c:3056;C:3056;i:s2_challenge_contracts", "AEC_CONTRACT_WZ_TITLE;Timed objective", {0x50000C1}, "contract_ch_wz"},
		{0x0800F06E, 2500, 100, "t:CONTRACT;c:3057;C:3057;i:s2_challenge_contracts", "AEC_CONTRACT_DAGGER_TITLE;Timed objective", {0x50000C2}, "contract_ch_dagger"},
		// Owner decision (build/research/hq-economy-slice1-report.md, "Retail alignment"):
		// retail sells the 16 CWL team packs for 500 CoD Points, but this store has no CoD
		// Points economy, so they stay deliberately purchasable at 1000 Armory Credits -
		// currency 6, the same as the two rare drops above and the currency
		// hq_marketplace::purchase debits. Every row below therefore keeps the default
		// sku::currency. The CoD Points glyph on the Quartermaster's CWL tab is NOT SKU
		// data: ui_s2_quartermaster_cwl_menu_uc.dec.lua binds prices[1].value straight into
		// the tile's CoDPointsPrice model field and never reads prices[1].currency, while
		// QuarterMasterUtils.ProcessSkuInfo only fills CoDPointsPrice for currency 2. The
		// sku_details page the tile opens is currency-driven and renders Armory Credits.
		{0x200010c, 1000, 100, "t:CWL_EF;l:0x200010c|1", "Echo Fox Pack;5 CWL cosmetics", {0x200010c, 0x240042b, 0x6632175, 0x7000098, 0x7040004}},
		{0x2000117, 1000, 100, "t:CWL_ENVY;l:0x2000117|1", "Team Envy Pack;5 CWL cosmetics", {0x2000117, 0x240042c, 0x6632181, 0x7000099, 0x704000f}},
		{0x200010d, 1000, 100, "t:CWL_EPSI;l:0x200010d|1", "Epsilon Pack;5 CWL cosmetics", {0x200010d, 0x240042d, 0x6632176, 0x700009a, 0x7040005}},
		{0x200010e, 1000, 100, "t:CWL_EU;l:0x200010e|1", "eUnited Pack;5 CWL cosmetics", {0x200010e, 0x240042e, 0x6632178, 0x700009b, 0x7040006}},
		{0x200010f, 1000, 100, "t:CWL_EVIL;l:0x200010f|1", "Evil Geniuses Pack;5 CWL cosmetics", {0x200010f, 0x240042f, 0x6632179, 0x700009c, 0x7040007}},
		{0x2000110, 1000, 100, "t:CWL_FAZE;l:0x2000110|1", "FaZe Clan Pack;5 CWL cosmetics", {0x2000110, 0x2400430, 0x663217a, 0x700009d, 0x7040008}},
		{0x2000111, 1000, 100, "t:CWL_LUMI;l:0x2000111|1", "Luminosity Pack;5 CWL cosmetics", {0x2000111, 0x2400431, 0x663217b, 0x700009f, 0x7040009}},
		{0x2000112, 1000, 100, "t:CWL_MIND;l:0x2000112|1", "Mindfreak Pack;5 CWL cosmetics", {0x2000112, 0x2400432, 0x663217c, 0x70000a0, 0x704000a}},
		{0x2000113, 1000, 100, "t:CWL_OPT;l:0x2000113|1", "OpTic Gaming Pack;5 CWL cosmetics", {0x2000113, 0x2400433, 0x663217d, 0x70000a1, 0x704000b}},
		{0x2000114, 1000, 100, "t:CWL_RED;l:0x2000114|1", "Red Reserve Pack;5 CWL cosmetics", {0x2000114, 0x2400434, 0x663217e, 0x70000a2, 0x704000c}},
		{0x2000115, 1000, 100, "t:CWL_RISE;l:0x2000115|1", "Rise Nation Pack;5 CWL cosmetics", {0x2000115, 0x2400435, 0x663217f, 0x70000a3, 0x704000d}},
		{0x2000116, 1000, 100, "t:CWL_SPLY;l:0x2000116|1", "Splyce Pack;5 CWL cosmetics", {0x2000116, 0x2400436, 0x6632180, 0x70000a4, 0x704000e}},
		{0x200011a, 1000, 100, "t:CWL_UNI;l:0x200011a|1", "Unilad Pack;5 CWL cosmetics", {0x200011a, 0x2400437, 0x6632184, 0x70000a5, 0x7040012}},
		{0x2000119, 1000, 100, "t:CWL_VITA;l:0x2000119|1", "Team Vitality Pack;5 CWL cosmetics", {0x2000119, 0x2400439, 0x6632183, 0x70000a6, 0x7040011}},
		{0x2000118, 1000, 100, "t:CWL_KALI;l:0x2000118|1", "Team Kaliber Pack;5 CWL cosmetics", {0x2000118, 0x2400438, 0x6632182, 0x700009e, 0x7040010}},
		{0x200012f, 1000, 100, "t:CWL_CWL;l:0x200012f|1", "CWL Pack;5 CWL cosmetics", {0x200012f, 0x240042a, 0x6632177, 0x7000097, 0x7040013}},
	};
	// Retail capture 2026-09-13: native loot rarity column 29 AND StatsTable Group column 0.
	inline constexpr std::uint32_t rarity_prices[]{125, 275, 600, 7300, 8900};
	inline std::uint32_t collection_price(unsigned rarity, std::string_view type)
	{
		if (rarity >= std::size(rarity_prices)) rarity = 0;
		// Retail sells class camos one tier above column 29 (0: 250, 1: 550 AC); 2 was not captured.
		if (type == "weapon_class_camo" && rarity < 2) ++rarity;
		const bool camo = type == "weapon_camo" || type == "weapon_class_camo" || type == "universal_camo";
		const bool weapon = type.starts_with("weapon") && !camo && type != "weapon_charm" &&
			type != "weapon_reticle" && type != "weapon_attachment" && type != "weapon_grenade";
		if (rarity == 1 && camo) return 250;
		if (rarity == 2)
		{
			if (camo) return 550;
			if (type == "weapon_charm") return 2275;
			if (weapon || type == "costume" || type == "uniforms") return 3250;
		}
		if (rarity >= 3 && weapon) return 8900;
		return rarity_prices[rarity];
	}
	void set_item_types(const std::map<std::uint32_t, std::string>& types);
	std::vector<sku> catalog();
	std::vector<std::uint32_t> granted_items(const sku& entry);
	std::optional<sku> find_sku(std::uint32_t id);
	void set_rarities(const std::map<std::uint32_t, unsigned>& rarities);
	bool parse_skus(byte_buffer* buffer, sku_request& request);
	std::vector<sku> sku_page(const sku_request& request);
	// Returns a BD error code; commits debit, item and replay receipt together.
	unsigned purchase(const std::string& transaction, std::uint32_t id, std::uint32_t quantity);
	bool context(byte_buffer* buffer);
	bool parse_skus(byte_buffer* buffer, inventory_request& request, bool* includes_local_sku = nullptr);
	bool parse_inventory(byte_buffer* buffer, inventory_request& request);
	std::vector<hq_economy::item> inventory_page(const hq_economy::state& data,
		const inventory_request& request, std::uint64_t now, bool expired = false);
	bool parse_put(byte_buffer* buffer, std::uint64_t local_user, std::vector<hq_economy::item>& items);
	bool parse_pawn(byte_buffer* buffer, std::string& transaction, std::vector<hq_economy::item>& items);
	bool put(const std::vector<hq_economy::item>& items);
	// Returns a BD error code; pays each payable record's copies at its duplicate value
	// in one transaction with the replay receipt.
	unsigned pawn(const std::string& transaction, const std::vector<hq_economy::item>& items);
	bool parse_consume(byte_buffer* buffer, std::string& transaction, std::vector<hq_economy::item>& items);
	// Returns a BD error code; takes every item's quantity off its row in one transaction
	// with the replay receipt, or nothing if any row holds too few.
	unsigned consume(const std::string& transaction, const std::vector<hq_economy::item>& items);
}
