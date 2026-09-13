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
		{6, 1000, 100, "t:ZM", "LUA_MENU_RARE_ZOMBIE_SUPPLY_DROP;3 random items", {}, "", true},
		// SKUType.Quartermaster (100), not SKUType.Contracts (201). The shipped LUI defines
		// SKUType.Contracts but never fetches it: DwDataUtils._fetchSKUData only calls
		// Engine.Inventory_FetchAllSKUs(controller, SKUType.Quartermaster) and
		// DwDataUtils._updateSKUData only calls Engine.Inventory_GetAllSKUIDs(SKUType.Quartermaster),
		// and every contract lookup - QuarterMasterUtils.GetContractCurrencies (key "c"), the
		// sku_details "VIEW CONTRACT" option (key "C") - walks that one cached list. A record
		// typed 201 in the native SKU cache (+0x04, written by hq_native::sku_lookup) is not a
		// Quartermaster SKU. The contract id lives in the SKU data string, not in the type.
		{0x0800F021, 100, 100, "t:CONTRACT;c:162;C:162;i:s2_challenge_contracts", "TDM Headshots Contract;Timed objective", {0x50F0001}, "contract_4_headshots_tdm"},
		{0x0800F022, 350, 100, "t:CONTRACT;c:561;C:561;i:s2_challenge_contracts", "SMG Kill Contract;Timed objective", {0x50F0002}, "contract_50_kills_smg"},
		{0x0800F023, 450, 100, "t:CONTRACT;c:146;C:146;i:s2_challenge_contracts", "TDM Kill Contract;Timed objective", {0x50F0003}, "contract_55_kills_tdm"},
		{0x0800F024, 5000, 100, "t:CONTRACT;c:3048;C:3048;i:s2_challenge_contracts", "LAD Machine Gun Contract;Timed objective", {0x50F0004}, "contract_ch_lad"},
		{0x0800F025, 350, 100, "t:CONTRACT;c:149;C:149;i:s2_challenge_contracts", "Kill Contract;Timed objective", {0x50F0005}, "contract_45_kills"},
		{0x0800F026, 100, 100, "t:CONTRACT;c:153;C:153;i:s2_challenge_contracts", "Domination Kill Contract;Timed objective", {0x50F0006}, "contract_25_kills_dom"},
		{0x0800F027, 350, 100, "t:CONTRACT;c:164;C:164;i:s2_challenge_contracts", "Headshots Contract;Timed objective", {0x50F0007}, "contract_9_headshots"},
		{0x0800F028, 100, 100, "t:CONTRACT;c:204;C:204;i:s2_challenge_contracts", "LMG Kill Contract;Timed objective", {0x50F0008}, "contract_25_kills_lmg"},
		{0x0800F029, 450, 100, "t:CONTRACT;c:562;C:562;i:s2_challenge_contracts", "LMG Supply Contract;Timed objective", {0x50F0009}, "contract_50_kills_lmg"},
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
	bool pawn(const std::string& transaction, const std::vector<hq_economy::item>& items);
}
