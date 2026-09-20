#include <std_include.hpp>
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
		for (const auto& row : hq_zombies_catalog::entries)
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
		achievement_engine::set_loot_catalog({0x20000D});
		require(!ok(request(R"({"Action":"open_supply_drop","SupplyDropID":"sd_zombie_rare","ClientTx":"drop"})")), "missing Zombies pool fails closed");
		require(hq_economy::snapshot().inventory.at({6,0}).quantity == 1, "failed opening retains drop");
		achievement_engine::set_loot_catalog({0x4A00003}, true);
		achievement_engine::set_loot_catalog({});
		require(!ok(request(R"({"Action":"open_supply_drop","SupplyDropID":"sd_zombie_rare","ClientTx":"drop"})")), "missing regular-card pool fails closed");
		require(hq_economy::snapshot().inventory.at({6,0}).quantity == 1, "either missing pool retains drop");
		achievement_engine::set_loot_catalog({0x20000D});
		auto opened = request(R"({"Action":"open_supply_drop","SupplyDropID":"sd_zombie_rare","ClientTx":"drop"})");
		require(ok(opened) && opened["GrantedItems"].Size() == 5, "ZM drop returns five reveal records");
		for (unsigned i = 0; i < 5; ++i)
			require(opened["GrantedItems"][i]["id"].GetUint() == (i < 2 ? 0x20000Du : 0x4A00003u), "two regular items then three consumables");
		std::ofstream(scratch / "zombies-drop-response.json") << achievement_engine::dispatch(R"({"Action":"open_supply_drop","SupplyDropID":"sd_zombie_rare","ClientTx":"drop"})");
		std::cout << "Reveal fixture: " << (scratch / "zombies-drop-response.json").string() << '\n';
		require(ok(request(R"({"Action":"open_supply_drop","SupplyDropID":"sd_zombie_rare","ClientTx":"drop"})")), "open replay");
		require(hq_economy::snapshot().inventory.at({0x4A00003,0}).quantity == 3, "three consumables, once");
		require(hq_economy::snapshot().inventory.at({0x20000D,0}).quantity == 2, "two regular cards, once");
		require(hq_marketplace::purchase("zm-purchase", 6, 1) == 0, "buy zombie drop");
		require(hq_marketplace::purchase("zm-purchase", 6, 1) == 0, "purchase replay");
		require(hq_economy::snapshot().currencies.at(6) == 500, "purchase debits AC once");
		require(hq_economy::transact([](auto& next) { return hq_mail::redeem(next, 1, "s2x-mail:welcome-v1"); }), "mail claim");
		require(hq_economy::transact([](auto& next) { return hq_mail::redeem(next, 1, "s2x-mail:welcome-v1"); }), "mail replay");
		require(hq_economy::snapshot().currencies.at(6) == 1000, "mail pays once");
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
			rules.emplace(definition.name, hq_event_predicate::rule{34, ""});
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
		require(contracts["Achievements"].Size() == 3, "three native Zombies contracts offered");
		for (const auto& entry : contracts["Achievements"].GetArray())
			require(entry["kind"].GetInt() == 11 && entry["expirationTimestamp"].GetUint64() == 0 &&
				entry["usageTimeTarget"].GetUint() > 0, "contract uses match-time expiry");
		const auto zm_first = hq_zombies_contract_catalog::achievement(hq_zombies_contract_catalog::entries[0]);
		require(!ok(transition("activate_user_contract", zm_first, "unpaid")), "contract cannot activate without paid token");
		const auto wallet_before_contracts = hq_economy::snapshot().currencies.at(6);
		for (const auto& definition : hq_zombies_contract_catalog::entries)
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

		achievement_engine::set_catalog({mp});
		auto mp_offers = request(R"({"Action":"get_scheduled_user_achievements"})");
		for (const auto& entry : mp_offers["Achievements"].GetArray()) require(entry["kind"].GetInt() < 8, "MP catalog excludes persisted ZM orders");
		std::cout << "PASS: native ZM kinds, quotas, event predicates/replay, claims/bonuses, MP isolation, legacy merge, rollover, purchases, mail, drops, persistence, co-op queue/envelopes\n";
		return 0;
	}
	catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
