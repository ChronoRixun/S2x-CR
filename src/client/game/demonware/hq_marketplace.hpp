#pragma once

#include "hq_economy.hpp"
#include "byte_buffer.hpp"

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
		// Promotional text = "<nameKey>;<descriptionKey>"; both halves go through
		// Engine.Localize (ui_s2_quartermaster_supply_drop_uc.dec.lua). These two name keys
		// are the game's own QuarterMasterUtils.SupplyDropPromoText entries for the MP and
		// Zombies rare drop tiles. No shipped key describes them, so the description half is
		// omitted: LUI.Split then yields nil and ProcessSkuInfo stores "".
		{2, 1000, 100, "t:MP", "LUA_MENU_RARE_SUPPLY_DROP"},
		{6, 1000, 100, "t:ZM", "LUA_MENU_RARE_ZOMBIE_SUPPLY_DROP"},
	};
	// Guessed local AC prices indexed by native item rarity (column29):
	// common, rare, legendary, epic, heroic. Retail prices not recovered.
	inline constexpr std::uint32_t rarity_prices[]{50, 250, 1000, 3000, 5000};
	std::vector<sku> catalog();
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
