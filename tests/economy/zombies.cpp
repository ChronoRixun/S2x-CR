#include <std_include.hpp>
#include <span>
#include <set>
#include "game/demonware/achievement_engine.hpp"
#include "game/demonware/hq_marketplace.hpp"
#include "game/demonware/hq_contract_catalog.hpp"
#include "game/demonware/hq_protocol.hpp"
#include "game/demonware/hq_mail.hpp"
#include "game/demonware/hq_vendor.hpp"
#include "game/demonware/hq_payroll.hpp"
#include "game/demonware/hq_products.hpp"
#include "game/types/demonware.hpp"

using namespace game::demonware;
#include "game/demonware/hq_item_data.hpp"
#include "game/demonware/hq_inventory_cache.hpp"
#include "game/demonware/hq_event_relay.hpp"
#include "game/demonware/byte_buffer.hpp"
#include "game/demonware/data_types.hpp"
#include "game/demonware/reply.hpp"
#include "game/demonware/achievement_store.hpp"
#include <utils/io.hpp>
namespace utils::flags
{
	bool has_flag(const std::string&) { return false; }
}
namespace game::environment
{
	bool is_zombies() { return true; }
}

namespace demonware
{
	std::uint64_t service_reply::transaction_id = 0;
	std::string captured_reply;
	void remote_reply::send(byte_buffer* buffer, const bool encrypted)
	{
		if (!encrypted) throw std::runtime_error("expected encrypted service reply");
		captured_reply = buffer->get_buffer();
	}
}
namespace utils::io {
bool read_file(const std::string& path, std::string* result) {
 std::ifstream file(path, std::ios::binary); if (!file) return false;
 result->assign(std::istreambuf_iterator<char>(file), {}); return !file.bad();
}
}
namespace utils::io {
bool write_file(const std::string& path, const std::string& data, bool append) {
 std::filesystem::create_directories(std::filesystem::path(path).parent_path());
 std::ofstream file(path, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
 file.write(data.data(), data.size()); return bool(file);
}
}
namespace demonware {
const char* get_achievement_status_name(achievement_status) { return "finished"; }
namespace achievement_store { std::vector<achievement_record> get_all() {
 achievement_record record{}; record.name="zombies_preserved"; record.kind=5;
 return {record};
} }
}
using namespace demonware;
void require(bool condition, const char* label) { if (!condition) throw std::runtime_error(label); }
rapidjson::Document request(const std::string& json) {
 auto result=achievement_engine::dispatch(json); rapidjson::Document d; d.Parse(result.c_str());
 require(!d.HasParseError(), "valid response JSON"); return d;
}

#include "game/demonware/hq_zombies_catalog.hpp"
#include "game/demonware/hq_zombies_contract_catalog.hpp"
#include "game/demonware/hq_contract_clock.hpp"
#include "game/demonware/hq_relay_queue.hpp"

bool ok(const rapidjson::Document& response)
{
	return response.HasMember("Status") && std::string_view{response["Status"].GetString()} == "ok";
}

rapidjson::Document transition(const char* action, const hq_economy::achievement& entry, const std::string& tx)
{
	return request(std::string{"{\"Action\":\""} + action + "\",\"AchievementName\":\"" + entry.name +
		"\",\"AchievementKind\":" + std::to_string(entry.kind) + ",\"ClientTx\":\"" + tx + "\"}");
}

// StructBuffer fixture: two indistinguishable headshot kills in the same second,
// separated by a malformed event. Native transaction field follows the events.
std::string varint(std::uint64_t value)
{
    std::string result;
    do { result += static_cast<char>((value & 127) | (value > 127 ? 128 : 0)); value >>= 7; } while (value);
    return result;
}
std::string blob(unsigned field, const std::string& value)
{
    return varint((field << 3) | 2) + varint(value.size()) + value;
}
std::vector<reward_game_events::event> collision_batch(std::int64_t timestamp, const std::string& tx, std::uint64_t user = 42)
{
    const auto parameter = [](unsigned selector, unsigned value) {
        return blob(4, blob(1, std::to_string(selector)) + varint(16) + varint(value));
    };
    const auto kill = blob(1, "zombies_kills") + varint(24) + varint(timestamp * 2) + parameter(1, 6) + parameter(6, 1);
    const auto invalid = blob(1, "zombies_kills"); // Missing timestamp: skipped without renumbering later events.
    const auto account = varint(8) + varint(user) + blob(2, "steam");
    const auto payload = blob(1, "s2_steam") + blob(2, blob(1, account) + blob(2, kill) + blob(2, invalid) + blob(2, kill) + (tx.empty() ? "" : blob(3, tx)));
    byte_buffer buffer; buffer.write_struct(const_cast<char*>(payload.data()), static_cast<int>(payload.size()));
    byte_buffer input{buffer.get_buffer()};
    std::vector<reward_game_events::user_event_batch> users;
    require(reward_game_events::parse_report_for_users_request(&input, users, true), "native collision batch parses");
    require(users.size() == 1 && users[0].events.size() == 2, "malformed neighbor isolated");
    return users[0].events;
}

void expanded_catalog_checks(std::int64_t& timestamp)
{
	// Hand-authored native kill samples, keyed by retail identity. These are not
	// generated from the predicates: dropping a condition must fail a negative case.
	using parameters = std::vector<reward_game_events::parameter>;
	const std::map<unsigned, parameters> samples{
		{1025, {{"6", 1}}}, {1028, {{"1", 4}}}, {1031, {{"1", 6}}},
		{1034, {{"1", 3}}}, {1016, {{"7", 1}}}, {1039, {{"3", 3}}},
		{1072, {{"6", 1}}}, {1068, {{"1", 8}}}, {1105, {{"1", 11}}},
		{1001, {{"2", 2}, {"3", 2}}}, {1014, {{"1", 5}, {"6", 1}}},
		{1010, {{"1", 11}}}, {1053, {{"1", 6}, {"7", 1}}},
		{1011, {{"2", 4}, {"128", 128}}}, {1040, {{"128", 4096}}},
		{1102, {{"128", 1073741824}}}, {1004, {{"2", 3}, {"128", 512}}},
		{1037, {{"128", 256}}}, {1005, {{"128", 64}}},
		{1047, {{"128", 134217728}}}, {1024, {{"2", 4}, {"128", 256}}},
		{1049, {{"128", 8192}}}, {1023, {{"2", 6}, {"128", 4194304}}},
		{1060, {{"128", 32}}}, {1063, {{"2", 5}}},
		{1067, {{"128", 8388608}}}, {1071, {{"5", 3}}},
		{1074, {}}, {1075, {}}, {1080, {}},
		{1077, {{"128", 4096 | 16777216}}}, {1081, {{"128", 33554432}}},
		{1082, {{"128", 134217728}}}, {1084, {{"128", 2}}}, {1088, {{"2", 9}}},
	};
	std::vector<hq_economy::achievement> full;
	std::map<std::string, hq_event_predicate::rule> rules;
	for (const auto& row : hq_zombies_catalog::entries)
	{
		full.push_back(hq_zombies_catalog::achievement(row));
		rules.emplace(row.name, hq_event_predicate::rule{34, row.predicate});
	}
	for (const auto& row : hq_zombies_contract_catalog::entries)
	{
		full.push_back(hq_zombies_contract_catalog::achievement(row));
		rules.emplace(row.name, hq_event_predicate::rule{34, row.predicate});
	}
	require(full.size() == 35 && samples.size() == full.size(), "27 orders and eight contract samples");
	achievement_engine::set_catalog(full);
	achievement_engine::set_event_rules(rules);
	std::set<std::string> seen_daily, seen_weekly;
	for (unsigned period = 0; period < 20; ++period)
	{
		hq_economy::state rotated;
		require(achievement_engine::reconcile_offers(rotated, 20000 + period), "fresh daily rotation");
		unsigned dailies{}, weeklies{};
		for (const auto& [name, entry] : rotated.achievements)
		{
			if (entry.kind == 8) { ++dailies; seen_daily.insert(name); }
			if (entry.kind == 9) ++weeklies;
		}
		require(dailies == 6 && weeklies == 3, "expanded pool retains six/three offer limits");
		rotated = {};
		require(achievement_engine::reconcile_offers(rotated, 20000 + period * 7), "fresh weekly rotation");
		for (const auto& [name, entry] : rotated.achievements)
			if (entry.kind == 9) seen_weekly.insert(name);
	}
	require(seen_daily.size() == 20 && seen_weekly.size() == 7, "rotation reaches every daily and weekly");
	// An accepted order survives pool expansion and rollover with its progress.
	hq_economy::state carried;
	auto old = hq_zombies_catalog::achievement(hq_zombies_catalog::entries[0]);
	old.status = "inProgress"; old.progress = 12; old.activation = 1234; old.offer_day = 19999;
	carried.achievements[old.name] = old;
	require(achievement_engine::reconcile_offers(carried, 20000), "carry legacy order into expanded pool");
	require(carried.achievements.at(old.name).progress == 12 &&
		carried.achievements.at(old.name).activation == 1234, "expansion preserves accepted progress");

	const auto check = [&](unsigned id, const hq_economy::achievement& entry, unsigned sku, unsigned price)
	{
		// Each fixture uses a fresh Zombies offer set inside the disposable test
		// profile. MP records, wallet and inventory remain in place.
		require(hq_economy::transact([](auto& next) {
			std::erase_if(next.achievements, [](const auto& pair) {
				return pair.second.kind == 8 || pair.second.kind == 9 || pair.second.kind == 11 || pair.first.starts_with("zm_above_beyond");
			});
			return hq_economy::grant(next, {"GRANT_CURRENCY", 6, 1000});
		}), "reset isolated Zombies offers");
		achievement_engine::set_catalog({entry});
		const auto before = hq_economy::snapshot();
		const auto tx = "expanded-" + std::to_string(id);
		if (sku)
		{
			require(!ok(transition("activate_user_contract", entry, tx)), "expanded contract requires payment");
			require(hq_marketplace::purchase(tx, sku, 1) == 0 && hq_marketplace::purchase(tx, sku, 1) == 0, "expanded purchase and replay");
		}
		require(ok(transition(sku ? "activate_user_contract" : "activate_scheduled_user_achievement", entry, tx)), "expanded option activates");
		timestamp = std::max(timestamp, static_cast<std::int64_t>(time(nullptr)) * 1000000 + 1);
		const auto& fields = samples.at(id);
		std::vector<reward_game_events::event> misses{{"1", timestamp++, fields}, {"37", timestamp++, fields}};
		for (std::size_t i = 0; i < fields.size(); ++i)
		{
			auto missing = fields; missing.erase(missing.begin() + i);
			misses.push_back({"34", timestamp++, missing});
			if (fields[i].selector == "128")
			{
				for (std::uint64_t bit = 1; bit <= fields[i].value; bit <<= 1)
					if (fields[i].value & bit)
					{
						auto wrong = fields; wrong[i].value &= ~bit;
						misses.push_back({"34", timestamp++, wrong});
					}
			}
			else
			{
				auto wrong = fields; ++wrong[i].value;
				misses.push_back({"34", timestamp++, wrong});
			}
		}
		require(achievement_engine::submit_events(misses), "nonmatching fixture accepted");
		require(hq_economy::snapshot().achievements.at(entry.name).progress == 0, "wrong type, missing fields and partial conditions do not progress");
		reward_game_events::event hit{"zombies_kills", timestamp++, fields};
		// Native flag words can include unrelated flags alongside the required bits.
		for (auto& field : hit.parameters) if (field.selector == "128") field.value |= 1;
		require(achievement_engine::submit_event(hit) && achievement_engine::submit_event(hit), "matching kill and retry accepted");
		hq_economy::invalidate();
		require(hq_economy::snapshot().achievements.at(entry.name).progress == 1, "matching kill persists exactly once");
		std::vector<reward_game_events::event> kills;
		for (unsigned count = 1; count < entry.target; ++count) kills.push_back({"34", timestamp++, fields});
		require(achievement_engine::submit_events(kills), "complete expanded objective");
		require(hq_economy::snapshot().achievements.at(entry.name).status == "claimable", "expanded option becomes claimable");
		require(ok(transition("claim_achievement_reward", entry, tx)) && ok(transition("claim_achievement_reward", entry, tx)), "expanded reward and replay");
		const auto after = hq_economy::snapshot();
		const auto currency_reward = entry.kind == 8 ? 250u : 0u;
		require(after.currencies.at(6) == before.currencies.at(6) - price + currency_reward, "expanded price and reward paid exactly once");
		if (entry.kind != 8) require(after.inventory.at({6, 0}).quantity == before.inventory.at({6, 0}).quantity + 1, "weekly/contract grants one Zombies drop");
	};
	for (const auto& row : hq_zombies_catalog::entries) check(row.id, hq_zombies_catalog::achievement(row), 0, 0);
	for (const auto& row : hq_zombies_contract_catalog::entries) check(row.id, hq_zombies_contract_catalog::achievement(row), row.sku, row.price);
	achievement_engine::set_catalog(full);
	std::cout << "PASS: 20 daily / 7 weekly rotation, carried progress, all 35 objectives, negative predicates, paid contracts, rewards and persistence\n";
}

void duplicate_drop_checks()
{
	constexpr std::uint32_t cosmetic = 0x20000D, consumable = 0x4A00003;
	const auto saved = hq_economy::snapshot();
	const auto seed = [&](unsigned drop, unsigned quantity, unsigned expires, unsigned credits) {
		require(hq_economy::transact([&](auto& next) {
			next.inventory.clear(); next.currencies.clear();
			std::erase_if(next.transactions, [](const auto& pair) { return !pair.first.starts_with("migration:"); });
			next.inventory[{cosmetic, 0}] = {cosmetic, quantity, 0, 0, expires};
			return hq_economy::grant(next, {"GRANT_PRODUCT", drop, 2}) &&
				hq_economy::grant(next, {"GRANT_PRODUCT", consumable, 4}) &&
				hq_economy::grant(next, {"GRANT_CURRENCY", 6, credits});
		}), "duplicate fixture seeded");
	};
	const auto open = [](const char* drop, const char* tx) {
		return request(std::string{"{\"Action\":\"open_supply_drop\",\"SupplyDropID\":\""} + drop + "\",\"ClientTx\":\"" + tx + "\"}");
	};
	// Deliberately synthetic per-item lookup results, not asserted retail prices.
	// The game-thread adapter supplies values from the native pawnValues reader.
	for (const auto price : {1u, 37u, 125u, 777u, 0u})
		for (const auto& [drop, name] : std::map<unsigned, const char*>{{1, "sd_mp"}, {2, "sd_mp_rare"}, {6, "sd_zombie_rare"}})
			for (const auto ownership : {0u, 1u, 2u, 3u})
			{
				// unowned, already owned, zero quantity, expired rental
				seed(drop, ownership == 2 ? 0 : ownership == 0 ? 0 : 1,
					ownership == 3 ? static_cast<unsigned>(time(nullptr) - 1) : 0, 100);
				achievement_engine::set_loot_catalog({cosmetic}, {{cosmetic, price}}, {{cosmetic, 1}});
				achievement_engine::set_zombies_loot_catalog({{consumable, {consumable, 1}}});
				const auto result = open(name, "duplicate-roll");
				require(ok(result), "MP common/rare and Zombies drop succeeds");
				const auto count = drop == 6 ? 2u : 3u;
				const auto paid = (count - (ownership == 1 ? 0 : 1)) * price;
				require(result["GrantedItems"].Size() == (drop == 6 ? 5 : 3), "converted duplicate still occupies its reveal card");
				for (unsigned i = 0; i < result["GrantedItems"].Size(); ++i)
					require(result["GrantedItems"][i]["id"].GetUint() == (i < count ? cosmetic : consumable), "reveal ordering preserved");
				const auto state = hq_economy::snapshot();
				require(state.currencies.at(6) == 100 + paid && state.inventory.at({cosmetic, 0}).quantity == 1 &&
					state.inventory.at({cosmetic, 0}).expires == 0, "one permanent cosmetic and exact duplicate credit");
				require(state.inventory.at({drop, 0}).quantity == 1, "one drop consumed");
				require(state.inventory.at({consumable, 0}).quantity == (drop == 6 ? 7 : 4), "owned Zombies consumables keep stacking");
				const auto& currencies = result["GrantedCurrencies"];
				require(currencies.Size() == (paid ? 1 : 0), "zero payout is not a spurious currency grant");
				if (paid) require(currencies[0]["currency_id"].GetUint() == 6 &&
					currencies[0]["balance_before"].GetUint() == 100 && currencies[0]["balance_delta"].GetUint() == paid,
					"GrantedCurrencies uses the native decoder fields and aggregate amount");
				// Replay after spending everything and unloading metadata must use the
				// original receipt, never reroll, reprice or grant the old payout again.
				require(hq_economy::transact([](auto& next) { return hq_economy::grant(next, {"SET_CURRENCY_BALANCE", 6, 0}); }), "spend after opening");
				achievement_engine::set_loot_catalog({});
				achievement_engine::set_zombies_loot_catalog({});
				hq_economy::invalidate();
				const auto revision = hq_economy::snapshot().revision;
				const auto replay = open(name, "duplicate-roll");
				require(ok(replay) && replay["GrantedItems"] == result["GrantedItems"] &&
					replay["GrantedCurrencies"] == currencies, "persisted receipt replays the same cards and credit report");
				require(hq_economy::snapshot().currencies.at(6) == 0 && hq_economy::snapshot().revision == revision,
					"replay neither repays nor writes the store");
				require(!ok(open(drop == 6 ? "sd_mp" : "sd_zombie_rare", "duplicate-roll")), "drop-type transaction conflict refused");
			}

	// An owned item without a pawn value still opens the drop: the duplicate card is
	// shown, nothing is credited and nothing is stacked. An unowned one is granted.
	seed(6, 1, 0, 100);
	achievement_engine::set_loot_catalog({cosmetic}, {}, {{cosmetic, 1}});
	achievement_engine::set_zombies_loot_catalog({{consumable, {consumable, 1}}});
	{
		const auto result = open("sd_zombie_rare", "missing-value");
		require(ok(result) && result["GrantedItems"].Size() == 5 && result["GrantedCurrencies"].Size() == 0,
			"missing pawn value opens the drop and converts nothing");
		const auto state = hq_economy::snapshot();
		require(state.currencies.at(6) == 100 && state.inventory.at({cosmetic, 0}).quantity == 1 &&
			state.inventory.at({6, 0}).quantity == 1 && state.inventory.at({consumable, 0}).quantity == 7,
			"unconvertible duplicate keeps wallet and cosmetic, consumes the drop, stacks consumables");
	}
	seed(6, 0, 0, 100);
	require(ok(open("sd_zombie_rare", "missing-value-unowned")) && hq_economy::snapshot().inventory.at({cosmetic, 0}).quantity == 1,
		"an unowned item without a pawn value is still granted once");
	// A rated card stacks its charge count, and a family with an unrated stock row
	// (Self-Revives) stacks there while the reveal still names the card.
	seed(6, 0, 0, 100);
	achievement_engine::set_zombies_loot_catalog({{0x4A0003F, {0x4A0003B, 4}}});
	{
		const auto result = open("sd_zombie_rare", "stock-row");
		require(ok(result) && result["GrantedItems"].Size() == 5 && !result.HasMember("StockRows"), "stock-row drop opens without exposing its stock note");
		for (unsigned i = 2; i < 5; ++i)
			require(result["GrantedItems"][i]["id"].GetUint() == 0x4A0003Fu, "reveal names the rated card");
		const auto state = hq_economy::snapshot();
		require(state.inventory.at({0x4A0003B, 0}).quantity == 12 && state.inventory.find({0x4A0003F, 0}) == state.inventory.end(),
			"three epic cards stack twelve units on the stock row and none on the card");
		bool stock_reported{}, card_reported{};
		for (const auto& item : result["DetailedInventory"].GetArray())
		{
			if (item["item_id"].GetUint() == 0x4A0003Bu) stock_reported = item["item_quantity"].GetUint() == 12;
			if (item["item_id"].GetUint() == 0x4A0003Fu) card_reported = item["item_quantity"].GetUint() == 0;
		}
		require(stock_reported && card_reported, "DetailedInventory carries the stock row and the empty card");
		// A retry after the catalog is gone must still answer from the receipt.
		achievement_engine::set_zombies_loot_catalog({});
		const auto replay = open("sd_zombie_rare", "stock-row");
		require(ok(replay) && replay["GrantedItems"] == result["GrantedItems"] && !replay.HasMember("StockRows"),
			"a retry without the catalog replays the receipt and keeps its stock note private");
		bool replay_stock{};
		for (const auto& item : replay["DetailedInventory"].GetArray())
			if (item["item_id"].GetUint() == 0x4A0003Bu) replay_stock = item["item_quantity"].GetUint() == 12;
		require(replay_stock && hq_economy::snapshot().inventory.at({0x4A0003B, 0}).quantity == 12,
			"the retry reports the stock row and grants nothing again");
	}
	achievement_engine::set_zombies_loot_catalog({{consumable, {consumable, 1}}});
	achievement_engine::set_loot_catalog({cosmetic}, {{cosmetic, 25}}, {{cosmetic, 1}});
	auto revision = hq_economy::snapshot().revision;

	seed(6, 1, 0, UINT32_MAX - 1);
	revision = hq_economy::snapshot().revision;
	require(!ok(open("sd_zombie_rare", "overflow")) && hq_economy::snapshot().revision == revision &&
		hq_economy::snapshot().inventory.at({6, 0}).quantity == 2 && hq_economy::snapshot().inventory.at({consumable, 0}).quantity == 4,
		"wallet overflow rolls back consumed drop and all five rolls");

	seed(6, 1, 0, 100);
	revision = hq_economy::snapshot().revision;
	const auto blocked_save = CreateFileA("players2/user/hq_economy.json.tmp", GENERIC_WRITE, 0,
		nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	require(blocked_save != INVALID_HANDLE_VALUE, "hold temporary save file exclusively");
	const auto failed = open("sd_zombie_rare", "save-failure");
	CloseHandle(blocked_save);
	hq_economy::invalidate();
	require(!ok(failed) && hq_economy::snapshot().revision == revision &&
		hq_economy::snapshot().inventory.at({6, 0}).quantity == 2 && hq_economy::snapshot().currencies.at(6) == 100,
		"failed save leaves the original drop and balance on disk");
	require(ok(open("sd_zombie_rare", "save-failure")) && hq_economy::snapshot().currencies.at(6) == 150,
		"failed-save retry pays duplicates once");
	// Receipts from before duplicate conversion remain valid and do not gain a
	// retroactive payout merely because a client retries an old opening.
	require(hq_economy::transact([](auto& next) {
		next.transactions["drop:legacy"] = R"({"Status":"ok","SupplyDropID":"sd_zombie_rare","GrantedItems":[{"id":2097165},{"id":2097165},{"id":77594627},{"id":77594627},{"id":77594627}],"GrantedCurrencies":[]})";
		return true;
	}), "legacy receipt fixture");
	const auto legacy = open("sd_zombie_rare", "legacy");
	require(ok(legacy) && legacy["GrantedCurrencies"].Empty() && hq_economy::snapshot().currencies.at(6) == 150,
		"old receipt replay never invents retroactive duplicate credits");
	require(hq_economy::transact([&](auto& next) { next = saved; return true; }), "restore prior economy fixture");
	std::cout << "PASS: MP/ZM duplicate payouts, same-drop repeats, zero/expired ownership, consumable stacking, native currency fields, persisted replay, missing metadata, overflow and save rollback\n";
}

void tier_drop_checks()
{
	// One synthetic item per tier, Common to Heroic. Without pawn values a repeat only shows.
	constexpr std::uint32_t items[]{0x2000010, 0x2000011, 0x2000012, 0x2000013, 0x2000014};
	const auto saved = hq_economy::snapshot();
	const auto seed = [](unsigned drop, unsigned count) {
		require(hq_economy::transact([&](auto& next) {
			next.inventory.clear(); next.currencies.clear();
			std::erase_if(next.transactions, [](const auto& pair) { return !pair.first.starts_with("migration:"); });
			return hq_economy::grant(next, {"GRANT_PRODUCT", drop, count});
		}), "tier fixture seeded");
	};
	const auto open = [](const char* drop, const std::string& tx) {
		return request(std::string{"{\"Action\":\"open_supply_drop\",\"SupplyDropID\":\""} + drop + "\",\"ClientTx\":\"" + tx + "\"}");
	};
	std::map<std::uint32_t, unsigned> tier;
	for (unsigned i = 0; i < 5; ++i) tier[items[i]] = i;
	achievement_engine::set_loot_catalog({std::begin(items), std::end(items)}, {}, tier);
	achievement_engine::set_zombies_loot_catalog({{0x4A00003, {0x4A00003, 1}}});
	// Weights 45/30/15/8/2, and 0/60/25/12/3 for a Rare drop's first card. Each order
	// check below is several standard deviations wide at 500 drops.
	unsigned first[5]{}, rest[5]{};
	seed(2, 500);
	for (unsigned i = 0; i < 500; ++i)
	{
		const auto result = open("sd_mp_rare", "tier-" + std::to_string(i));
		require(ok(result) && result["GrantedItems"].Size() == 3, "tiered Rare drop opens three cards");
		for (unsigned card = 0; card < 3; ++card) ++(card ? rest : first)[tier.at(result["GrantedItems"][card]["id"].GetUint())];
	}
	require(!first[0] && first[1] > first[2] && first[2] > first[3] && first[3] > first[4] && first[4],
		"a Rare drop's first card is Rare or better, Heroic included");
	require(rest[0] > rest[1] && rest[1] > rest[2] && rest[2] > rest[3] && rest[3] > rest[4] && rest[4],
		"other cards follow the Common-heavy weights");
	// Empty tiers re-roll among the rest: with only Common and Heroic items, the floored
	// card is always Heroic, and the Zombies reveal keeps its two-plus-three shape.
	achievement_engine::set_loot_catalog({items[0], items[4]}, {}, tier);
	seed(6, 20);
	for (unsigned i = 0; i < 20; ++i)
	{
		const auto result = open("sd_zombie_rare", "sparse-" + std::to_string(i));
		require(ok(result) && result["GrantedItems"].Size() == 5 && result["GrantedItems"][0]["id"].GetUint() == items[4] &&
			result["GrantedItems"][4]["id"].GetUint() == 0x4A00003u, "empty tiers re-roll into the Heroic floor card");
	}
	// Nothing Rare or better (an unrated item counts as Common): the floor cannot hold,
	// so the Rare drop is refused and kept, while a common drop still opens.
	achievement_engine::set_loot_catalog({items[0], items[4]}, {}, {{items[0], 0}});
	seed(2, 1);
	require(!ok(open("sd_mp_rare", "no-rare")) && hq_economy::snapshot().inventory.at({2, 0}).quantity == 1,
		"Rare drop refused and kept without a Rare-or-better item");
	seed(1, 1);
	require(ok(open("sd_mp", "common-only")) && !hq_economy::snapshot().inventory.at({1, 0}).quantity, "common drop opens from Common items");
	require(hq_economy::transact([&](auto& next) { next = saved; return true; }), "restore prior economy fixture");
	std::cout << "PASS: tier-then-item drops, Rare-or-better first card incl. Heroic, empty-tier re-roll, refused floor keeps the drop\n";
}

void item_data_receipt_checks()
{
	const auto saved = hq_economy::snapshot();
	const std::vector<hq_item_data::update> seen{{0x7000001, 0, "\x01"}}, other{{0x7000001, 0, "\x02"}};
	require(hq_economy::transact([](auto& next) { next.inventory[{0x7000001, 0}] = {0x7000001, 1}; return true; }), "item-data fixture");
	require(hq_item_data::apply("legacy", seen), "item-data write");
	require(hq_economy::transact([](auto& next) {
		auto& value = next.transactions.at("item-data:legacy");
		value = value.substr(value.find(':', 9) + 1); // bare fingerprint, as written before the bound
		return true;
	}), "legacy item-data receipt fixture");
	require(hq_item_data::apply("legacy", seen) && !hq_item_data::apply("legacy", other), "legacy receipt still guards its replay");
	const auto count = [] { return std::ranges::count_if(hq_economy::snapshot().transactions, [](const auto& entry) { return entry.first.starts_with("item-data:"); }); };
	for (unsigned i = 0; i < 256; ++i) require(hq_item_data::apply("seen-" + std::to_string(i), seen), "item-data writes");
	require(count() == 256 && !hq_economy::snapshot().transactions.contains("item-data:legacy"), "legacy receipt retires first");
	require(hq_item_data::apply("seen-256", seen) && count() == 256 && !hq_economy::snapshot().transactions.contains("item-data:seen-0"), "oldest receipt retires at the bound");
	const auto revision = hq_economy::snapshot().revision;
	require(hq_item_data::apply("seen-1", seen) && !hq_item_data::apply("seen-256", other) && hq_economy::snapshot().revision == revision, "kept receipts replay once and refuse a different request");
	hq_economy::invalidate();
	require(count() == 256 && hq_economy::snapshot().inventory.at({0x7000001, 0}).metadata == "\x01", "bounded receipts persist");
	require(hq_economy::transact([&](auto& next) { next = saved; return true; }), "restore prior economy fixture");
	std::cout << "PASS: item-data receipts keep the newest 256, retire legacy first, and still guard replays\n";
}

void consume_receipt_checks()
{
	const auto saved = hq_economy::snapshot();
	constexpr std::uint32_t card = 0x4A00001; // Max Ammo (common)
	const std::vector<hq_economy::item> one{{card, 1}}, two{{card, 2}};
	require(hq_economy::transact([&](auto& next) { next.inventory[{card, 0}] = {card, 1000}; return true; }), "consume fixture");
	require(hq_marketplace::consume("legacy", one) == BD_NO_ERROR, "card use");
	require(hq_economy::transact([](auto& next) {
		auto& value = next.transactions.at("consume:legacy");
		value = value.substr(value.find(':', 9) + 1); // bare fingerprint, as written before the bound
		return true;
	}), "legacy consume receipt fixture");
	const auto quantity = [&] { return hq_economy::snapshot().inventory.at({card, 0}).quantity; };
	require(hq_marketplace::consume("legacy", one) == BD_NO_ERROR && hq_marketplace::consume("legacy", two) == BD_MARKETPLACE_RESOURCE_CONFLICT &&
		quantity() == 999, "legacy receipt still guards its replay");
	const auto count = [] { return std::ranges::count_if(hq_economy::snapshot().transactions, [](const auto& entry) { return entry.first.starts_with("consume:"); }); };
	for (unsigned i = 0; i < 256; ++i) require(hq_marketplace::consume("use-" + std::to_string(i), one) == BD_NO_ERROR, "card uses");
	require(count() == 256 && !hq_economy::snapshot().transactions.contains("consume:legacy"), "legacy receipt retires first");
	require(hq_marketplace::consume("use-256", one) == BD_NO_ERROR && count() == 256 && !hq_economy::snapshot().transactions.contains("consume:use-0"),
		"oldest receipt retires at the bound");
	const auto revision = hq_economy::snapshot().revision;
	require(hq_marketplace::consume("use-1", one) == BD_NO_ERROR && hq_marketplace::consume("use-256", two) == BD_MARKETPLACE_RESOURCE_CONFLICT &&
		hq_economy::snapshot().revision == revision && quantity() == 742, "kept receipts replay and refuse a different request without charging");
	hq_economy::invalidate();
	require(count() == 256 && quantity() == 742, "bounded receipts persist");
	require(hq_economy::transact([&](auto& next) { next = saved; return true; }), "restore prior economy fixture");
	std::cout << "PASS: consume receipts keep the newest 256, retire legacy first, and still guard replays\n";
}

void zombies_prestige_checks(std::int64_t& timestamp)
{
	const auto saved = hq_economy::snapshot();
	const auto prestige = [&](const std::uint64_t level) { return achievement_engine::submit_event({"11", timestamp++, {{"5", level}}}); };
	const auto added = [&] { return hq_economy::snapshot().inventory.size() - saved.inventory.size(); };
	const auto cards = [](const std::uint32_t count)
	{
		const auto state = hq_economy::snapshot();
		for (std::uint32_t n = 1; n <= count; ++n)
		{
			const auto card = state.inventory.find({0x240025B + n, 0});
			if (card == state.inventory.end() || card->second.quantity != 1 || !state.transactions.contains("prestige:zm:" + std::to_string(n))) return false;
		}
		return true;
	};
	require(prestige(3) && cards(3) && added() == 3, "Zombies prestige 3 pays cards 1..3");
	require(prestige(3) && added() == 3, "a second prestige 3 event pays nothing more");
	require(prestige(10) && cards(10) && added() == 10, "prestige 10 pays cards 4..10 and nothing else");
	require(prestige(0) && prestige(11) && added() == 10, "levels 0 and 11 pay nothing");
	hq_economy::invalidate();
	require(cards(10), "the cards and their receipts persist");
	require(hq_economy::transact([&](auto& next) { next = saved; return true; }), "restore prior economy fixture");
	std::cout << "PASS: a Zombies prestige pays every calling card up to its level once, nothing for levels 0 or 11\n";
}

void mp_weapon_contract_checks(std::int64_t& timestamp)
{
	// The MP board is nine consecutive catalog rows, one row further each day. A weapon
	// contract pays its Rare loot0 (a synthetic GUID here) and is priced by its own SKU.
	const auto saved = hq_economy::snapshot();
	constexpr auto size = std::size(hq_contract_catalog::entries);
	const auto sku_for = [](const hq_contract_catalog::definition& row) {
		return std::find_if(std::begin(hq_marketplace::vendor_skus), std::end(hq_marketplace::vendor_skus),
			[&](const auto& sku) { return std::string_view{sku.contract} == row.name; });
	};
	std::vector<hq_economy::achievement> catalog;
	std::map<std::string, hq_event_predicate::rule> rules;
	std::set<std::uint32_t> tokens;
	for (std::size_t i = 0; i < size; ++i)
	{
		const auto& row = hq_contract_catalog::entries[i];
		const auto sku = sku_for(row);
		require(sku != std::end(hq_marketplace::vendor_skus) && std::count_if(std::begin(hq_marketplace::vendor_skus),
			std::end(hq_marketplace::vendor_skus), [&](const auto& other) { return std::string_view{other.contract} == row.name; }) == 1 &&
			sku->price == row.price && std::string_view{sku->data}.find("c:" + std::to_string(row.id) + ";") != std::string_view::npos,
			"each MP contract has one SKU with its price and id");
		tokens.insert(hq_marketplace::granted_items(*sku).front());
		if (*row.item_reference)
			require(row.currency == 0 && row.amount == 1 && row.price == 2500 &&
				std::string_view{row.item_reference}.ends_with("_loot0_mp"), "weapon contract grants one Rare loot0 for 2500 AC");
		catalog.push_back(hq_contract_catalog::achievement(row, 0x7F00000 + static_cast<unsigned>(i)));
		rules.emplace(row.name, hq_event_predicate::rule{1, ""});
	}
	require(tokens.size() == size, "cost tokens distinct");
	for (std::size_t start = 0; start < size; ++start)
	{
		bool generic{};
		for (std::size_t i = 0; i < 9; ++i) generic |= !*hq_contract_catalog::entries[(start + i) % size].item_reference;
		require(generic, "every day's board holds a generic contract");
	}
	require(hq_economy::transact([](auto& next) {
		std::erase_if(next.achievements, [](const auto& pair) { return pair.second.kind == 4; });
		next.inventory.clear(); next.currencies.clear();
		return hq_economy::grant(next, {"GRANT_CURRENCY", 6, 2500});
	}), "reset MP contracts");
	achievement_engine::set_event_rules(rules);
	achievement_engine::set_catalog(catalog);
	const auto day = static_cast<std::uint64_t>(time(nullptr)) / 86400;
	auto board = request(R"({"Action":"get_scheduled_user_achievements","AchievementKind":4})");
	require(ok(board) && board["Achievements"].Size() == 9, "nine MP contracts offered");
	std::size_t first = size;
	for (unsigned i = 0; i < 9; ++i)
	{
		const auto index = (day + i) % size;
		require(std::string_view{board["Achievements"][i]["name"].GetString()} == hq_contract_catalog::entries[index].name, "board is today's nine consecutive rows");
		if (first == size && *hq_contract_catalog::entries[index].item_reference) first = index;
	}
	require(first < size, "today's board holds a weapon contract");
	const auto& weapon = catalog[first];
	const auto sku = sku_for(hq_contract_catalog::entries[first])->id;
	require(!ok(transition("activate_user_contract", weapon, "weapon")), "weapon contract requires payment");
	require(hq_marketplace::purchase("weapon", sku, 1) == 0 && hq_marketplace::purchase("weapon", sku, 1) == 0, "weapon contract purchase and replay");
	require(hq_economy::snapshot().currencies.at(6) == 0, "weapon contract debits 2500 AC once");
	require(ok(transition("activate_user_contract", weapon, "weapon")), "paid weapon contract activates");
	timestamp = std::max(timestamp, static_cast<std::int64_t>(time(nullptr)) * 1000000 + 1);
	for (unsigned i = 0; i < weapon.target; ++i) require(achievement_engine::submit_event({"1", timestamp++, {}}), "weapon contract event");
	require(hq_economy::snapshot().achievements.at(weapon.name).status == "claimable", "weapon contract claimable");
	require(ok(transition("claim_achievement_reward", weapon, "weapon-claim")) && ok(transition("claim_achievement_reward", weapon, "weapon-claim")), "weapon contract claim and replay");
	require(hq_economy::snapshot().inventory.at({weapon.rewards.front().id, 0}).quantity == 1, "claim grants the weapon once");
	// Rows 9..55 from today's start are off the board: not for sale, but one bought on an
	// earlier day still progresses and pays.
	std::size_t k = 9;
	while (!*hq_contract_catalog::entries[(day + k) % size].item_reference) ++k;
	const auto off = (day + k) % size;
	require(hq_economy::transact([](auto& next) { return hq_economy::grant(next, {"GRANT_CURRENCY", 6, 2500}); }), "fund off-board purchase");
	require(hq_marketplace::purchase("off-board", sku_for(hq_contract_catalog::entries[off])->id, 1) != 0 &&
		hq_economy::snapshot().currencies.at(6) == 2500, "off-board weapon contract is not for sale");
	auto carried = catalog[off];
	require(hq_economy::transact([&](auto& next) {
		carried.status = "inProgress"; carried.offer_day = day - (size - k);
		carried.activation = static_cast<std::uint64_t>(time(nullptr)); carried.activation_generation = next.revision + 1;
		next.achievements[carried.name] = carried;
		return true;
	}), "contract bought before it rotated off");
	bool listed{};
	for (const auto& entry : request(R"({"Action":"get_user_achievements"})")["Achievements"].GetArray())
		listed |= std::string_view{entry["name"].GetString()} == carried.name;
	require(listed, "off-board contract still listed");
	timestamp = std::max(timestamp, static_cast<std::int64_t>(time(nullptr)) * 1000000 + 1);
	for (unsigned i = 0; i < carried.target; ++i) require(achievement_engine::submit_event({"1", timestamp++, {}}), "off-board contract event");
	require(hq_economy::snapshot().achievements.at(carried.name).status == "claimable", "off-board contract claimable");
	require(ok(transition("claim_achievement_reward", carried, "off-board-claim")), "off-board contract claim");
	require(hq_economy::snapshot().inventory.at({carried.rewards.front().id, 0}).quantity == 1, "off-board claim grants its weapon");
	require(hq_economy::transact([&](auto& next) { next = saved; return true; }), "restore prior economy fixture");
	std::cout << "PASS: " << size << "-row MP contract board, one SKU and token each, weapon contract purchase/activation/claim, off-board purchase refused and off-board progress paid\n";
}

void mp_variant_order_checks(std::int64_t& timestamp)
{
	// A retail variant daily pays its own Epic/Heroic GUID. Door Kicker counts shotgun kills only.
	const auto saved = hq_economy::snapshot();
	const auto& row = *std::find_if(std::begin(hq_contract_catalog::orders), std::end(hq_contract_catalog::orders),
		[](const auto& entry) { return entry.id == 680; });
	const auto order = hq_contract_catalog::achievement(row);
	require(order.kind == 1 && order.target == 100 && order.rewards.size() == 1 && order.rewards.front().type == "GRANT_PRODUCT" &&
		order.rewards.front().id == 0x1023400 && order.rewards.front().amount == 1, "Door Kicker is a daily for 100 kills paying the Epic M30");
	achievement_engine::set_event_rules({{row.name, {1, "(1:4)"}}});
	achievement_engine::set_catalog({order});
	require(ok(transition("activate_scheduled_user_achievement", order, "variant")), "variant daily activates");
	timestamp = std::max(timestamp, static_cast<std::int64_t>(time(nullptr)) * 1000000 + 1);
	require(achievement_engine::submit_event({"1", timestamp++, {{"1", 2}}}) &&
		hq_economy::snapshot().achievements.at(order.name).progress == 0, "an SMG kill does not count");
	std::vector<reward_game_events::event> kills;
	for (unsigned i = 0; i < order.target; ++i) kills.push_back({"1", timestamp++, {{"1", 4}}});
	require(achievement_engine::submit_events(kills) && hq_economy::snapshot().achievements.at(order.name).status == "claimable", "shotgun kills complete it");
	const auto before = hq_economy::snapshot();
	const auto owned = before.inventory.contains({0x1023400, 0}) ? before.inventory.at({0x1023400, 0}).quantity : 0;
	require(ok(transition("claim_achievement_reward", order, "variant-claim")) && ok(transition("claim_achievement_reward", order, "variant-claim")), "variant claim and replay");
	hq_economy::invalidate();
	const auto after = hq_economy::snapshot();
	require(after.inventory.at({0x1023400, 0}).quantity == owned + 1 && after.currencies == before.currencies, "claim grants the variant once and no currency");
	require(hq_economy::transact([&](auto& next) { next = saved; return true; }), "restore prior economy fixture");
	std::cout << "PASS: variant daily progresses on its weapon class only and pays its GUID once\n";
}

void social_rank_checks(std::int64_t timestamp)
{
	// Social rank N is social_score {1 = N}: sent at the threshold and again on every achievements fetch.
	const auto saved = hq_economy::snapshot();
	const auto held = [](const std::uint32_t id) {
		const auto state = hq_economy::snapshot(); const auto entry = state.inventory.find({id, 0});
		return entry == state.inventory.end() ? 0u : entry->second.quantity;
	};
	const auto credits = [] { const auto state = hq_economy::snapshot(); return state.currencies.contains(6) ? state.currencies.at(6) : 0u; };
	const auto rank = [&](const char* selector, const std::uint64_t value) { return reward_game_events::event{"27", timestamp++, {{selector, value}}}; };
	const auto ac = credits(), drops = held(1), card = held(0x240026F), variant = held(0x1012200), mark = held(0x8000D1);
	std::vector<reward_game_events::event> batch{rank("1", 1), rank("1", 2), rank("1", 3)};
	batch[0].name = "social_score";
	require(achievement_engine::submit_events(batch), "social ranks 1-3 accepted");
	const auto paid = [&] { return credits() == ac + 500 && held(0x240026F) == card + 1 && held(1) == drops + 1; };
	require(paid(), "ranks 1-3 pay 500 AC, the rank 2 card and a common drop");
	require(achievement_engine::submit_events(batch) && achievement_engine::submit_events({rank("1", 1), rank("1", 2), rank("1", 3)}), "replay and catch-up accepted");
	require(paid(), "replay and catch-up pay nothing more");
	require(achievement_engine::submit_events({rank("1", 0), rank("1", 21), rank("2", 5)}), "bad ranks and selectors accepted");
	require(achievement_engine::submit_event(rank("1", 20)) && achievement_engine::submit_event(rank("1", 20)), "rank 20 accepted twice");
	require(paid() && held(0x1012200) == variant && held(0x8000D1) == mark + 1, "rank 20 pays once; bad ranks and selectors pay nothing");
	std::vector<reward_game_events::event> noise;
	for (unsigned i = 0; i < 2100; ++i) noise.push_back({"37", timestamp++, {{"1", 1}}});
	require(achievement_engine::submit_events(noise) && achievement_engine::submit_events(batch), "first batch again after 2,100 events");
	hq_economy::invalidate();
	require(paid() && held(0x8000D1) == mark + 1 && std::ranges::count_if(hq_economy::snapshot().transactions,
		[](const auto& entry) { return entry.first.starts_with("social:rank:"); }) == 4, "rank receipts outlive the event ring and persist");
	require(hq_economy::transact([&](auto& next) { next = saved; return true; }), "restore prior economy fixture");
	std::cout << "PASS: social ranks pay once; replay, catch-up, event-ring eviction and reload pay nothing more; bad ranks pay nothing\n";
}

void promo_pack_checks()
{
	const auto saved = hq_economy::snapshot();
	constexpr std::uint32_t preorder = 0x6003001, endowment = 0x600002f, zombies = 0x700013f;
	const std::vector<std::uint32_t> uniforms{0x6003001, 0x6003003, 0x6003009, 0x600300d, 0x6003010};
	require(hq_economy::transact([](auto& next) { next.inventory.clear(); return hq_economy::grant(next, {"SET_CURRENCY_BALANCE", 6, 20000}); }), "promo pack fixture");
	const auto credits = [] { return hq_economy::snapshot().currencies.at(6); };
	const auto pack = hq_marketplace::find_sku(preorder);
	require(pack && pack->price == 16250 && hq_marketplace::granted_items(*pack) == uniforms, "pre-order pack price and items");
	require(hq_marketplace::purchase("promo-1", preorder, 1) == 0 && credits() == 3750, "pack purchase debits once");
	for (const auto id : uniforms) require(hq_economy::snapshot().inventory.at({id, 0}).quantity == 1, "pack grants every uniform once");
	require(hq_marketplace::purchase("promo-1", preorder, 1) == 0 && credits() == 3750, "pack replay debits nothing");
	require(hq_marketplace::purchase("promo-2", preorder, 1) != 0 && credits() == 3750, "owned pack refuses a rebuy");
	require(hq_marketplace::purchase("promo-3", endowment, 1) == 0 && credits() == 0, "second pack spends the rest");
	const auto revision = hq_economy::snapshot().revision;
	require(hq_marketplace::purchase("promo-4", zombies, 1) != 0 && hq_economy::snapshot().revision == revision, "short funds grant nothing");
	for (const auto& entry : hq_marketplace::vendor_skus)
	{
		const std::string_view data{entry.data};
		const auto limiter = data.find("l:");
		if (limiter == data.npos) continue;
		require(std::strtoul(data.data() + limiter + 2, nullptr, 16) == entry.id && entry.items[0] == entry.id, "limiter is the SKU id and first item");
		require(data.size() < 64 && std::string_view{entry.promotional_text}.size() < 64, "SKU data and promo text fit the native cache");
	}
	require(hq_economy::transact([&](auto& next) { next = saved; return true; }), "restore prior economy fixture");
	std::cout << "PASS: promo packs grant every item for one debit, replay once, refuse a rebuy or short funds, and every limited SKU is limited by its first item\n";
}

int main(int argc, char** argv)
{
	try
	{
		const auto scratch = std::filesystem::temp_directory_path() / ("s2x-zombies-tests-" +
			std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
		std::filesystem::create_directories(scratch);
		std::filesystem::current_path(scratch);
		const auto today = static_cast<std::uint64_t>(time(nullptr)) / 86400;
		hq_economy::achievement mp;
		mp.name = mp.challenge_name = "daily_ch_assault_kills";
		mp.target = 10; mp.rewards = {{"GRANT_PRODUCT", 1, 1}};
		achievement_engine::set_catalog({mp});
		require(ok(transition("activate_scheduled_user_achievement", mp, "mp")), "MP order activates");
		const auto mp_before = hq_economy::snapshot().achievements.at(mp.name);

		std::vector<hq_economy::achievement> catalog;
		std::map<std::string, hq_event_predicate::rule> rules;
		// Keep the original nine-order fixture stable; the full pool is checked below.
		for (const auto& row : std::span{hq_zombies_catalog::entries}.first<9>())
		{
			catalog.push_back(hq_zombies_catalog::achievement(row));
			rules.emplace(row.name, hq_event_predicate::rule{34, row.predicate});
		}
		achievement_engine::set_event_rules(rules);
		achievement_engine::set_catalog(catalog);
		auto daily = request(R"({"Action":"get_scheduled_user_achievements","AchievementKind":8})");
		auto weekly = request(R"({"Action":"get_scheduled_user_achievements","AchievementKind":9})");
		require(ok(daily) && daily["Achievements"].Size() == 6, "six native Zombies dailies");
		require(ok(weekly) && weekly["Achievements"].Size() == 3, "three native Zombies weeklies");
		require(request(R"({"Action":"get_scheduled_user_achievements","AchievementKinds":[1,2,4]})")["Achievements"].Empty(), "MP offers not leaked into Zombies catalog");
		require(achievement_engine::period_end(9, 20004) == achievement_engine::period_end(2, 20004), "native kind 9 gets weekly boundary");
		require(achievement_engine::period_end(8, 20004) == achievement_engine::period_end(1, 20004), "native kind 8 gets daily boundary");
		for (unsigned i = 0; i < 3; ++i) require(ok(transition("activate_scheduled_user_achievement", catalog[i], "activate")), "ZM activation");
		require(!ok(transition("activate_scheduled_user_achievement", catalog[3], "fourth")), "three active daily limit");
		require(ok(transition("activate_scheduled_user_achievement", catalog[6], "weekly")), "weekly quota independent of daily");
		require(ok(transition("deactivate_user_achievement", catalog[2], "abandon")), "ZM abandon");
		require(hq_economy::snapshot().achievements.at(catalog[2].name).status == "available", "abandoned daily stays offered");
		require(ok(transition("activate_scheduled_user_achievement", catalog[2], "again")), "ZM reaccept");

		std::int64_t timestamp = static_cast<std::int64_t>(time(nullptr)) * 1000000 + 1;
		reward_game_events::event kill{"34", timestamp++, {{"1", 6}, {"6", 1}, {"7", 1}}};
		require(achievement_engine::submit_event(kill), "native ZM kill");
		kill.name = "zombies_kills";
		std::reverse(kill.parameters.begin(), kill.parameters.end());
		require(achievement_engine::submit_event(kill), "cross-path replay accepted");
		auto state = hq_economy::snapshot();
		require(state.achievements.at(catalog[0].name).progress == 1 && state.achievements.at(catalog[2].name).progress == 1, "replay not double counted");
		require(state.achievements.at(catalog[1].name).progress == 0, "pistol kill not shotgun");
		require(state.achievements.at(catalog[6].name).progress == 1, "weekly native kind progresses");
		require(state.achievements.at(mp.name).progress == mp_before.progress && state.achievements.at(mp.name).activation == mp_before.activation, "ZM preserves active MP order");
		require(achievement_engine::submit_event({"37", timestamp++, {{"1", 1}}}), "unselected event acknowledged");
		require(hq_economy::snapshot().achievements.at(catalog[0].name).progress == 1, "wave records do not count as kills");

        const auto collision_time = ++timestamp;
        auto collisions = collision_batch(collision_time, "native-batch-A");
        require(collisions[0].occurrence == 0 && collisions[1].occurrence == 1, "same-second native multiplicity survives malformed neighbor");
        require(achievement_engine::submit_events(collisions), "native colliding kills accepted");
        require(hq_economy::snapshot().achievements.at(catalog[0].name).progress == 3, "both same-second headshots count");
        // The native SDK rebuilds the transaction ID when retrying queued events.
        require(achievement_engine::submit_events(collision_batch(collision_time, "native-batch-B")), "rebuilt native batch retry accepted");
        hq_economy::invalidate();
        require(hq_economy::snapshot().achievements.at(catalog[0].name).progress == 3, "persisted replay with new batch ID does not recount");
        for (auto event : collisions)
        {
            event.name = "34";
            std::reverse(event.parameters.begin(), event.parameters.end());
            const auto wire = hq_event_relay::encode(42, event);
            require(!wire.empty(), "occurrence relay encoded");
            hq_event_relay::receiver decoder;
            unsigned delivered{};
            for (const auto& part : hq_event_relay::chunks(42, wire))
                require(decoder.accept(part, 42, [&](const auto& decoded) {
                    ++delivered;
                    require(decoded.occurrence == event.occurrence, "relay preserves native occurrence");
                    return achievement_engine::submit_event(decoded);
                }), "occurrence relay fragments accepted");
            require(delivered == 1, "one reassembled delivery");
        }
        require(hq_economy::snapshot().achievements.at(catalog[0].name).progress == 3, "relay retry does not recount native kills");
        require(achievement_engine::submit_events(collision_batch(collision_time, "")), "retry without optional transaction accepted");
        require(hq_economy::snapshot().achievements.at(catalog[0].name).progress == 3, "missing optional transaction does not recount");
        auto invalid_occurrence = collisions[1]; invalid_occurrence.occurrence = 100;
        require(!achievement_engine::valid_event(invalid_occurrence) && hq_event_relay::encode(42, invalid_occurrence).empty(), "out of range occurrence rejected");
        invalid_occurrence.occurrence = 1; invalid_occurrence.name = "1";
        require(!achievement_engine::valid_event(invalid_occurrence), "occurrence scoped to Zombies kills");
        std::cout << "PASS: genuine same-second kills, native retries with new batch ID, persisted replay and fragmented co-op replay\n";

		// Complete every daily with the relevant native kill fields, checking claim
		// retries and the separate Zombies completion bonus all the way to disk.
		const unsigned weapon[] = {6, 4, 6, 3, 6, 8};
		unsigned cache_updates{};
		achievement_engine::set_cache_update_sink([&](auto) { ++cache_updates; });
		for (unsigned i = 0; i < 6; ++i)
		{
			require(ok(transition("activate_scheduled_user_achievement", catalog[i], "activate")), "accept daily");
			timestamp = std::max(timestamp, static_cast<std::int64_t>(time(nullptr)) * 1000000 + 1);
			for (unsigned count = 0; count < catalog[i].target; ++count)
				require(achievement_engine::submit_event({"34", timestamp++, {{"1", weapon[i]}, {"3", 3}, {"6", 1}, {"7", 1}}}), "count matching event");
			require(hq_economy::snapshot().achievements.at(catalog[i].name).status == "claimable", "daily claimable");
			const auto tx = "zm-claim-" + std::to_string(i);
			require(ok(transition("claim_achievement_reward", catalog[i], tx)), "daily reward granted");
			const auto paid = hq_economy::snapshot().currencies.at(hq_economy::armory_credits);
			require(ok(transition("claim_achievement_reward", catalog[i], tx)), "claim replay succeeds");
			require(hq_economy::snapshot().currencies.at(hq_economy::armory_credits) == paid, "claim replay pays once");
		}
		state = hq_economy::snapshot();
		require(state.currencies.at(6) == 1500 && !state.currencies.contains(1), "daily rewards are AC, not XP");
		require(state.inventory.at({6,0}).quantity == 1 && !state.inventory.contains({2,0}), "ZM bonus is zombie drop, not MP rare");
		require(state.achievements.at("zm_above_beyond_daily").progress == 6 && state.achievements.at("above_beyond_daily").progress == 0, "bonuses isolated by mode");
		require(cache_updates >= 6, "claims refresh native cache");
		auto user = request(R"({"Action":"get_user_achievements"})");
		bool legacy{};
		for (const auto& entry : user["Achievements"].GetArray()) legacy |= std::string_view{entry["name"].GetString()} == "zombies_preserved";
		require(legacy, "merged response preserves legacy Zombies unlocks");
		auto rolled = state;
		require(achievement_engine::reconcile_offers(rolled, today + 7), "ZM rollover");
		require(rolled.achievements.at("zm_above_beyond_daily").progress == 0, "ZM bonus resets at boundary");
		require(rolled.achievements.at(mp.name).offer_day == state.achievements.at(mp.name).offer_day, "ZM rollover leaves MP offers alone");

		// The Zombies reveal needs both pools: two regular cards then three
		// consumables. Missing either pool must leave the drop intact.
		achievement_engine::set_loot_catalog({0x20000D}, {{0x20000D, 25}}, {{0x20000D, 1}});
		require(!ok(request(R"({"Action":"open_supply_drop","SupplyDropID":"sd_zombie_rare","ClientTx":"drop"})")), "missing Zombies pool fails closed");
		require(hq_economy::snapshot().inventory.at({6,0}).quantity == 1, "failed opening retains drop");
		achievement_engine::set_zombies_loot_catalog({{0x4A00003, {0x4A00003, 1}}});
		achievement_engine::set_loot_catalog({});
		require(!ok(request(R"({"Action":"open_supply_drop","SupplyDropID":"sd_zombie_rare","ClientTx":"drop"})")), "missing regular-card pool fails closed");
		require(hq_economy::snapshot().inventory.at({6,0}).quantity == 1, "either missing pool retains drop");
		achievement_engine::set_loot_catalog({0x20000D}, {{0x20000D, 25}}, {{0x20000D, 1}});
		auto opened = request(R"({"Action":"open_supply_drop","SupplyDropID":"sd_zombie_rare","ClientTx":"drop"})");
		require(ok(opened) && opened["GrantedItems"].Size() == 5, "ZM drop returns five reveal records");
		for (unsigned i = 0; i < 5; ++i)
			require(opened["GrantedItems"][i]["id"].GetUint() == (i < 2 ? 0x20000Du : 0x4A00003u), "two regular items then three consumables");
		std::ofstream(scratch / "zombies-drop-response.json") << achievement_engine::dispatch(R"({"Action":"open_supply_drop","SupplyDropID":"sd_zombie_rare","ClientTx":"drop"})");
		std::cout << "Reveal fixture: " << (scratch / "zombies-drop-response.json").string() << '\n';
		require(ok(request(R"({"Action":"open_supply_drop","SupplyDropID":"sd_zombie_rare","ClientTx":"drop"})")), "open replay");
		require(hq_economy::snapshot().inventory.at({0x4A00003,0}).quantity == 3, "three consumables, once");
		require(hq_economy::snapshot().inventory.at({0x20000D,0}).quantity == 1, "two regular cards: one item and one converted duplicate");
		// A Zombies level-up pays a Rare Zombie Supply Drop once per event, as the
		// after-action screen promises; the receipt makes a retry a no-op.
		reward_game_events::event levelup{"14", timestamp++, {}};
		require(achievement_engine::submit_event(levelup) && achievement_engine::submit_event(levelup), "ZM level-up accepted and retried");
		require(hq_economy::snapshot().inventory.at({6,0}).quantity == 1 && !hq_economy::snapshot().inventory.contains({2,0}), "ZM level-up grants one zombie drop, not an MP rare, and only once");
		require(hq_marketplace::purchase("zm-purchase", 6, 1) == 0, "buy zombie drop");
		require(hq_marketplace::purchase("zm-purchase", 6, 1) == 0, "purchase replay");
		require(hq_economy::snapshot().currencies.at(6) == 525, "purchase debits AC once after duplicate credits");
		require(hq_economy::transact([](auto& next) { return hq_mail::redeem(next, 1, "s2x-mail:welcome-v1"); }), "mail claim");
		require(hq_economy::transact([](auto& next) { return hq_mail::redeem(next, 1, "s2x-mail:welcome-v1"); }), "mail replay");
		require(hq_economy::snapshot().currencies.at(6) == 1025, "mail pays once after duplicate credits");
		hq_economy::invalidate();
		require(hq_economy::snapshot().inventory.at({0x4A00003,0}).quantity == 3, "ZM inventory survives reload");

		// Reuse the MP bounded queue and codec; the outer tag prevents cross-mode delivery.
		hq_event_relay::server_queue queue;
		require(queue.push_batch({{42, kill}}, [] { return false; }) == reward_delivery::retryable_failure, "batch rollback on store failure");
		require(queue.take(48, [](auto) { return true; }).empty(), "failed batch not published");
		require(queue.push(42, kill) == reward_delivery::queued, "remote ZM event queued");
		hq_event_relay::receiver receiver;
		unsigned received{};
		for (auto& [id, fragment] : queue.take(48, [](auto) { return true; }))
		{
			require(hq_event_relay::tag_fragment(fragment, true), "tag Zombies fragment");
			auto wrong_mode = fragment;
			require(!hq_event_relay::untag_fragment(wrong_mode, false), "MP rejects ZM envelope");
			require(hq_event_relay::untag_fragment(fragment, true), "ZM accepts own envelope");
			receiver.accept(fragment, id, [&](const auto&) { ++received; return true; });
		}
		require(received == 1, "co-op event reassembled once");
		// Native kind-11 contracts share the transaction machinery, but have their
		// own quota and clocks. Keep three active MP contracts and a paid MP token.
		for (const auto& definition : hq_zombies_contract_catalog::entries)
		{
			catalog.push_back(hq_zombies_contract_catalog::achievement(definition));
			rules.emplace(definition.name, hq_event_predicate::rule{34, definition.predicate});
			const auto sku = hq_marketplace::find_sku(definition.sku);
			require(sku && sku->price == definition.price && sku->currency == 6 &&
				hq_marketplace::granted_items(*sku).front() == definition.token &&
				std::string_view{sku->contract} == definition.name, "ZM cost token and SKU match catalog");
		}
		achievement_engine::set_event_rules(rules);
		achievement_engine::set_catalog(catalog);
		require(hq_economy::transact([](auto& next) {
			for (unsigned i = 0; i < 3; ++i)
			{
				auto contract = hq_contract_catalog::achievement(hq_contract_catalog::entries[i]);
				contract.status = "inProgress"; contract.activation = time(nullptr);
				contract.activation_generation = next.revision + i + 1;
				next.achievements[contract.name] = contract;
			}
			return hq_economy::grant(next, {"GRANT_CURRENCY", 6, 2000}) &&
				hq_economy::grant(next, {"GRANT_PRODUCT", 0x50000B9, 1});
		}), "MP active contracts and pending purchase fixture");
		auto contracts = request(R"({"Action":"get_scheduled_user_achievements","AchievementKind":11})");
		require(contracts["Achievements"].Size() == 8, "eight native Zombies contracts offered");
		for (const auto& entry : contracts["Achievements"].GetArray())
			require(entry["kind"].GetInt() == 11 && entry["expirationTimestamp"].GetUint64() == 0 &&
				entry["usageTimeTarget"].GetUint() > 0, "contract uses match-time expiry");
		const auto zm_first = hq_zombies_contract_catalog::achievement(hq_zombies_contract_catalog::entries[0]);
		require(!ok(transition("activate_user_contract", zm_first, "unpaid")), "contract cannot activate without paid token");
		const auto wallet_before_contracts = hq_economy::snapshot().currencies.at(6);
		for (const auto& definition : std::span{hq_zombies_contract_catalog::entries}.first<3>())
		{
			const auto contract = hq_zombies_contract_catalog::achievement(definition);
			const auto tx = std::string{"buy-"} + definition.name;
			require(hq_marketplace::purchase(tx, definition.sku, 1) == 0, "ZM purchase ignores occupied MP slots");
			require(hq_marketplace::purchase(tx, definition.sku, 1) == 0, "contract purchase replay");
			require(hq_economy::snapshot().inventory.at({definition.token, 0}).quantity == 1, "paid token granted once");
			require(!ok(transition("activate_scheduled_user_achievement", contract, tx)), "free-order action cannot bypass contract activation");
			require(ok(transition("activate_user_contract", contract, tx)), "kind-11 contract activation");
			require(ok(transition("activate_user_contract", contract, tx)), "activation replay");
			require(hq_economy::snapshot().inventory.at({definition.token, 0}).quantity == 0, "activation consumes paid token once");
			require(hq_marketplace::purchase(tx + "-again", definition.sku, 1) != 0, "active contract cannot be repurchased");
		}
		require(hq_economy::snapshot().currencies.at(6) == wallet_before_contracts - 800, "three purchases debit AC exactly once");
		require(hq_marketplace::purchase("fourth-zm-slot", hq_zombies_contract_catalog::entries[3].sku, 1) != 0, "expanded catalog still limits Zombies to three active contracts");
		hq_contract_clock contract_clock;
		const auto start = hq_contract_clock::clock::now();
		contract_clock.tick(true, start, 11);
		contract_clock.tick(true, start + std::chrono::seconds{10}, 11);
		contract_clock.tick(false, start + std::chrono::seconds{1000}, 11);
		contract_clock.tick(false, start + std::chrono::seconds{2000}, 11);
		require(hq_economy::snapshot().achievements.at(zm_first.name).usage == 10, "ZM timer stops in lobby");
		contract_clock.tick(true, start + std::chrono::seconds{3000}, 11);
		contract_clock.tick(true, start + std::chrono::seconds{3005}, 11);
		require(hq_economy::snapshot().achievements.at(zm_first.name).usage == 15, "ZM timer resumes in match");
		for (unsigned i = 0; i < 3; ++i)
			require(hq_economy::snapshot().achievements.at(hq_contract_catalog::entries[i].name).usage == 0, "Zombies leaves MP clocks paused");
		timestamp = std::max(timestamp, static_cast<std::int64_t>(time(nullptr)) * 1000000 + 1);
		for (unsigned i = 0; i < zm_first.target; ++i)
			require(achievement_engine::submit_event({"34", timestamp++, {}}), "native kills progress Zombies contract");
		require(hq_economy::snapshot().achievements.at(zm_first.name).status == "claimable", "ZM contract reaches claimable");
		const auto drops_before_contract = hq_economy::snapshot().inventory.at({6, 0}).quantity;
		require(ok(transition("claim_achievement_reward", zm_first, "contract-claim")), "claim Zombies contract");
		require(ok(transition("claim_achievement_reward", zm_first, "contract-claim")), "contract claim replay");
		require(hq_economy::snapshot().inventory.at({6, 0}).quantity == drops_before_contract + 1, "contract awards one Zombies drop");
		require(hq_marketplace::purchase("finished-again", hq_zombies_contract_catalog::entries[0].sku, 1) != 0, "finished contract cannot be rebought same day");
		contract_clock.tick(true, start + std::chrono::seconds{7005}, 11);
		const auto zm_second = hq_zombies_contract_catalog::achievement(hq_zombies_contract_catalog::entries[1]);
		require(hq_economy::snapshot().achievements.at(zm_second.name).status == "expired", "ZM contract expires after match time");
		require(!ok(transition("claim_achievement_reward", zm_second, "expired-claim")), "expired contract cannot claim reward");
		const auto zm_third = hq_zombies_contract_catalog::achievement(hq_zombies_contract_catalog::entries[2]);
		require(ok(transition("deactivate_user_achievement", zm_third, "contract-abandon")), "ZM contract abandon");
		require(!ok(transition("activate_user_contract", zm_third, "contract-free-retry")), "abandon needs another paid token");
		hq_economy::invalidate();
		require(hq_economy::snapshot().achievements.at(zm_second.name).status == "expired" &&
			hq_economy::snapshot().inventory.at({6, 0}).quantity == drops_before_contract + 1, "contract expiry and rewards persist");
		std::cout << "PASS: native ZM contracts, SKU/token purchase, activation/claim replay, match timers, expiry, abandonment and MP contract isolation\n";

        // Optional owner-capture regression. Inputs stay outside the repository;
        // only the unique temporary test profile is written. This fixture is the
        // three wave reports from the 27-kill / 26-AAR-headshot match.
        if (argc == 5 && std::string_view{argv[1]} == "--captured-zm-match")
        {
            require(hq_economy::transact([&](auto& next) {
                next.transactions.clear();
                for (auto entry : catalog)
                {
                    entry.progress = 0; entry.activation = 0; entry.status = "inProgress";
                    next.achievements[entry.name] = entry;
                }
                return true;
            }), "isolated capture fixture");
            std::vector<reward_game_events::event> recorded;
            for (int i = 2; i < argc; ++i)
            {
                std::string bytes; require(utils::io::read_file(argv[i], &bytes), "read captured native request");
                byte_buffer buffer{bytes};
                std::vector<reward_game_events::user_event_batch> users;
                require(reward_game_events::parse_report_for_users_request(&buffer, users, true), "production parser accepts captured request");
                require(users.size() == 1, "solo capture has one player");
                recorded.insert(recorded.end(), users[0].events.begin(), users[0].events.end());
            }
            require(achievement_engine::submit_events(recorded), "apply captured match");
            require(achievement_engine::submit_events(recorded), "replay captured match");
            const auto progress = hq_economy::snapshot();
            require(progress.achievements.at(zm_first.name).progress == 27, "all 27 captured kills count exactly once");
            require(progress.achievements.at("daily_zm_ch_headshots_1").progress == 12 && progress.achievements.at("weekly_zm_ch_headshots_1").progress == 12, "12 native headshot flags preserved; no AAR compensation");
            require(progress.achievements.at("daily_zm_ch_lmgs_1").progress == 13, "all 13 LMG kills count including collision");
            require(progress.achievements.at("daily_zm_ch_pistols_1").progress == 11, "11 pistol kills isolated");
            require(progress.achievements.at("daily_zm_ch_kills_upgraded_1").progress == 1, "one native upgraded flag");
            for (const auto* name : {"daily_zm_ch_shotguns_1", "daily_zm_ch_explosives_1", "weekly_zm_ch_kills_equipment", "weekly_zm_ch_traps_1"})
                require(progress.achievements.at(name).progress == 0, "nonmatching orders unchanged");
            std::cout << "PASS: captured match twice: 27 kills, 12 headshots, 13 LMG, 11 pistol, 1 upgraded; other predicates unchanged\n";
        }

		expanded_catalog_checks(timestamp);
		duplicate_drop_checks();
		tier_drop_checks();
		item_data_receipt_checks();
		consume_receipt_checks();
		zombies_prestige_checks(timestamp);
		mp_weapon_contract_checks(timestamp);
		mp_variant_order_checks(timestamp);
		social_rank_checks(timestamp);
		promo_pack_checks();
		achievement_engine::set_catalog({mp});
		auto mp_offers = request(R"({"Action":"get_scheduled_user_achievements"})");
		for (const auto& entry : mp_offers["Achievements"].GetArray()) require(entry["kind"].GetInt() < 8, "MP catalog excludes persisted ZM orders");
		std::cout << "PASS: native ZM kinds, quotas, event predicates/replay, claims/bonuses, MP isolation, legacy merge, rollover, purchases, mail, drops, persistence, co-op queue/envelopes\n";
		return 0;
	}
	catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
