#include <std_include.hpp>
#include "achievement_engine.hpp"
#include "achievement_response.hpp"
#include "hq_protocol.hpp"
#include "hq_payroll.hpp"
#include "component/console/console.hpp"
#include "steam/steam.hpp"
#include <charconv>
#include <random>

namespace demonware::achievement_engine
{
	namespace
	{
		std::mutex catalog_mutex{};
		std::vector<hq_economy::achievement> definitions{};
		std::vector<std::uint32_t> loot_items{};
		using allocator = rapidjson::Document::AllocatorType;

		rapidjson::Value text(const std::string_view value, allocator& alloc)
		{
			return rapidjson::Value{value.data(), static_cast<rapidjson::SizeType>(value.size()), alloc};
		}

		std::string string(const rapidjson::Value& value, const char* key)
		{
			return value.HasMember(key) && value[key].IsString()
				? std::string{value[key].GetString(), value[key].GetStringLength()} : std::string{};
		}

		std::string encode(const rapidjson::Value& value)
		{
			rapidjson::StringBuffer buffer{};
			rapidjson::Writer<rapidjson::StringBuffer> writer{buffer};
			value.Accept(writer);
			return {buffer.GetString(), buffer.GetSize()};
		}

		// day = the UTC day index containing 'now'; the period boundary is derived from it
		// (not from the stored offer day) so the emitted end time is always in the future.
		rapidjson::Value serialize(const hq_economy::achievement& entry, allocator& alloc,
			const std::uint64_t day, const bool scheduled = false)
		{
			rapidjson::Value value{rapidjson::kObjectType};
			value.AddMember("name", text(entry.name, alloc), alloc);
			value.AddMember("challengeName", text(entry.challenge_name, alloc), alloc);
			value.AddMember("kind", entry.kind, alloc);
			const auto status = scheduled ? (entry.status == "inProgress" ? "in_progress" :
				entry.status == "finished" ? "completed" : entry.status.c_str()) :
				(entry.status == "available" ? "inactive" : entry.status.c_str());
			value.AddMember("status", text(status, alloc), alloc);
			value.AddMember("requiresClaim", true, alloc);
			value.AddMember("progress", entry.progress, alloc);
			value.AddMember("progressTarget", entry.target, alloc);
			value.AddMember("globalProgressTarget", 0, alloc);
			value.AddMember("globalCounterID", 0, alloc);
			value.AddMember("fulfilledTimes", entry.completion ? 1 : 0, alloc);
			value.AddMember("completionCount", entry.completion ? 1 : 0, alloc);
			value.AddMember("completionTimestamp", entry.completion, alloc);
			value.AddMember("activationTimestamp", entry.activation, alloc);
			// Both keys, because the two consumers disagree. The native record parser
			// (0x13EF20) fills the scheduled cache field at +0x18 from "expirationTimestamp"
			// - with only "eventEndTimestamp" present aecache reports expires 0 - while
			// "eventEndTimestamp" is the name that sits in the Achievement Engine string
			// block beside get_scheduled_user_achievements/NextPeriodStartTimes and is what
			// the last reply the Orders board rendered (run-61492) carried. Unknown members
			// are ignored by the parser, so publish the same value under both.
			const auto expires = period_end(entry.kind, day);
			value.AddMember("expirationTimestamp", expires, alloc);
			value.AddMember("eventEndTimestamp", expires, alloc);
			value.AddMember("usageTimeTarget", entry.usage_target, alloc);
			value.AddMember("usageTimeRemaining", entry.usage_target - std::min(entry.usage_target, entry.usage), alloc);
			rapidjson::Value rewards{rapidjson::kArrayType};
			for (const auto& reward : entry.rewards)
			{
				rapidjson::Value result{rapidjson::kObjectType};
				const auto product = reward.type == "GRANT_PRODUCT";
				if (!product && reward.type != "GRANT_CURRENCY") continue;
				result.AddMember("type", text(product ? "grant_product" : "grant_currency", alloc), alloc);
				rapidjson::Value payload{rapidjson::kObjectType};
				payload.AddMember("id", reward.id, alloc);
				if (!product) payload.AddMember("amount", reward.amount, alloc);
				else
				{
					payload.AddMember("currencies", rapidjson::Value{rapidjson::kArrayType}, alloc);
					rapidjson::Value items{rapidjson::kArrayType}, item{rapidjson::kObjectType};
					item.AddMember("id", reward.id, alloc);
					item.AddMember("quantity", reward.amount, alloc);
					items.PushBack(item, alloc);
					payload.AddMember("items", items, alloc);
				}
				result.AddMember(rapidjson::StringRef(product ? "product" : "currency"), payload, alloc);
				rewards.PushBack(result, alloc);
			}
			value.AddMember("successRewards", rewards, alloc);
			return value;
		}

		bool matches(const rapidjson::Value& request, const hq_economy::achievement& entry)
		{
			if (request.HasMember("AchievementKind") &&
				(!request["AchievementKind"].IsInt() || request["AchievementKind"].GetInt() != entry.kind)) return false;
			for (const auto* key : {"AchievementKinds", "AchievementNames", "AchievementStatuses"})
			{
				if (!request.HasMember(key)) continue;
				if (!request[key].IsArray()) return false;
				bool found{};
				for (const auto& value : request[key].GetArray())
				{
					if (std::strcmp(key, "AchievementKinds") == 0)
						found |= value.IsInt() && value.GetInt() == entry.kind;
					else if (value.IsString())
					{
						const std::string_view requested{value.GetString(), value.GetStringLength()};
						if (std::strcmp(key, "AchievementNames") == 0) found |= requested == entry.name;
						else
						{
							const auto scheduled = string(request, "Action") == "get_scheduled_user_achievements";
							const auto status = scheduled ? (entry.status == "inProgress" ? "in_progress" :
								entry.status == "finished" ? "completed" : entry.status.c_str()) :
								(entry.status == "available" ? "inactive" : entry.status.c_str());
							found |= requested == status;
						}
					}
				}
				if (!found) return false;
			}
			return true;
		}

		std::vector<hq_economy::achievement> offers(const std::uint64_t day)
		{
			std::lock_guard lock{catalog_mutex};
			std::vector<hq_economy::achievement> result{};
			for (const auto kind : {1, 2, 4})
			{
				std::vector<hq_economy::achievement> pool{};
				for (const auto& entry : definitions) if (entry.kind == kind) pool.push_back(entry);
				const auto period = kind == 2 ? day / 7 : day;
				for (std::size_t i = 0; i < std::min<std::size_t>(3, pool.size()); ++i)
				{
					auto entry = pool[(period + i) % pool.size()];
					entry.offer_day = kind == 2 ? period * 7 : day;
					result.push_back(entry);
				}
			}
			return result;
		}
	}

	std::uint64_t period_end(const int kind, const std::uint64_t day)
	{
		return (kind == 2 ? (day / 7 + 1) * 7 : day + 1) * 86400;
	}

	bool reconcile_offers(hq_economy::state& data, const std::uint64_t day)
	{
		std::lock_guard lock{catalog_mutex};
		bool changed{};
		for (const auto kind : {1, 2})
		{
			std::vector<hq_economy::achievement> pool;
			for (const auto& entry : definitions) if (entry.kind == kind) pool.push_back(entry);
			if (pool.empty()) continue; // table loading has not completed
			const auto current = [&](const auto& entry) { return kind == 2 ? entry.offer_day / 7 == day / 7 : entry.offer_day == day; };
			std::size_t live{};
			for (auto& [name, entry] : data.achievements)
			{
				if (entry.kind != kind) continue;
				if (entry.status == "inProgress" || entry.status == "claimable")
				{
					++live;
					// A carried order keeps progress/activation/reward and gets today's
					// offer date, so its UI expiration uses the current boundary.
					if (entry.offer_day != day) { entry.offer_day = day; changed = true; }
				}
				else if (entry.status == "available" && !current(entry))
				{ entry.status = "expired"; changed = true; }
			}
			// Preserve current available offers, including the just-abandoned one.
			for (auto& [name, entry] : data.achievements)
			{
				if (entry.kind != kind || entry.status != "available") continue;
				if (live >= 3) { entry.status = "expired"; changed = true; continue; }
				++live;
				if (entry.offer_day != day) { entry.offer_day = day; changed = true; }
			}
			const auto period = kind == 2 ? day / 7 : day;
			// Prefer unused definitions; if the small local pool is exhausted,
			// completed definitions may be offered again to maintain three slots.
			// The old receipt remains until activation, and cannot grant twice.
			for (const auto reuse_completed : {false, true})
				for (std::size_t i = 0; live < 3 && i < pool.size(); ++i)
				{
					auto entry = pool[(period + i) % pool.size()];
					const auto found = data.achievements.find(entry.name);
					if (found != data.achievements.end())
					{
						const auto& old = found->second;
						if (old.status == "available" || old.status == "inProgress" || old.status == "claimable") continue;
						if (!reuse_completed && old.status == "finished" && current(old)) continue;
						if (old.status == "finished" && current(old))
						{ entry.claim_transaction = old.claim_transaction; }
					}
					entry.offer_day = day; entry.status = "available";
					data.achievements[entry.name] = entry;
					++live; changed = true;
				}
		}
		return changed;
	}

	void set_catalog(std::vector<hq_economy::achievement> catalog)
	{
		std::lock_guard lock{catalog_mutex};
		definitions = std::move(catalog);
	}

	void set_loot_catalog(std::vector<std::uint32_t> items)
	{
		std::erase_if(items, [](const auto id) { return id <= 2 || id > INT32_MAX; });
		std::sort(items.begin(), items.end());
		items.erase(std::unique(items.begin(), items.end()), items.end());
		if (items.size() > 10000) items.clear();
		std::lock_guard lock{catalog_mutex};
		loot_items = std::move(items);
	}

	bool submit_event(const reward_game_events::event& event, const bool native_payroll)
	{
		const auto kills = event.name == "1" || event.name == "killed_a_player";
		const auto payroll = event.name == "18" || event.name == "picked_up_payroll";
		const auto end_game = event.name == "5" || event.name == "end_game";
		const auto multi_kill = event.name == "2" || event.name == "multi_kill";
		const auto streak = event.name == "4" || event.name == "streak";
		const auto duel = event.name == "7" || event.name == "one_v_one";
		const auto social = event.name == "10" || event.name == "social";
		if (!kills && !payroll && !end_game && !multi_kill && !streak && !duel && !social) return true;
		const auto now = static_cast<std::uint64_t>(time(nullptr));
		if (payroll && native_payroll)
		{
			// GrabPayroll is AEComplexEvents {event 18, key {1,2}, value {1, masterPrestige}};
			// Rank.GetPayrollAchievement returns 757 (payroll_officer_masterprestige) for a
			// master prestige player and 345 (payroll_officer) otherwise, and the kiosk
			// compares the pushed ID against it. The native ID is resolved from the record's
			// name (dw/dwGameChallenges.csv column 1, resolver 0x139D50), so publish the name
			// that player's kiosk waits for; the store keeps one payroll_officer entry.
			bool master_prestige{};
			for (const auto& parameter : event.parameters)
				if (parameter.selector == "2" && parameter.value) master_prestige = true;
			const auto* published = master_prestige ? "payroll_officer_masterprestige" : "payroll_officer";

			hq_payroll::push notification{};
			const auto ok = hq_economy::transact([&](hq_economy::state& data)
			{
				const auto before = data.currencies.contains(hq_economy::armory_credits) ? data.currencies.at(hq_economy::armory_credits) : 0;
				const auto result = hq_payroll::settle(data, event.timestamp, now);
				if (result == hq_payroll::outcome::rejected) return false;
				// A batch from another period is acknowledged but describes no pickup the
				// kiosk is waiting on, so it must not animate a collection.
				if (result == hq_payroll::outcome::stale) return true;
				const auto after = data.currencies.contains(hq_economy::armory_credits) ? data.currencies.at(hq_economy::armory_credits) : 0;
				// Published on a replay as well: the currency grant stays once per period, but
				// the kiosk arms a 5 s "Unable to get payroll at this time" banner on every
				// click and only an achievementEngine CompletionUpdate for this achievement
				// cancels it, so a second pickup inside the period needs the event too.
				auto entry = data.achievements.at("payroll_officer");
				entry.name = published; entry.challenge_name = published;
				// The pickup itself pays out, so the published copy always describes a
				// completed payroll even when the stored record is mid-claim; the store's
				// own status stays where the claim flow left it.
				entry.status = "finished"; entry.progress = entry.target;
				if (!entry.completion) entry.completion = now;
				rapidjson::Document push{rapidjson::kObjectType};
				auto& alloc = push.GetAllocator();
				auto record = serialize(entry, alloc, now / 86400);
				push.CopyFrom(record, alloc);
				push.AddMember("type", "CHALLENGE", alloc);
				push.AddMember("reason", "completed", alloc);
				// 0x13C480 raises the Lua event only when "triggers" is present, and a
				// SET_CURRENCY_BALANCE trigger must carry inventory.currencies; a replay
				// publishes the unchanged balance (delta 0) instead of omitting the array.
				rapidjson::Value triggers{rapidjson::kArrayType}, trigger{rapidjson::kObjectType};
				rapidjson::Value inventory{rapidjson::kObjectType}, currencies{rapidjson::kArrayType}, currency{rapidjson::kObjectType};
				currency.AddMember("currency_id", hq_economy::armory_credits, alloc);
				currency.AddMember("balance_before", before, alloc);
				currency.AddMember("balance_delta", after - before, alloc);
				currencies.PushBack(currency, alloc);
				inventory.AddMember("currencies", currencies, alloc);
				trigger.AddMember("type", "SET_CURRENCY_BALANCE", alloc);
				trigger.AddMember("inventory", inventory, alloc);
				triggers.PushBack(trigger, alloc);
				push.AddMember("triggers", triggers, alloc);
				notification.json = encode(push);
				notification.summary = std::string{published} + " kind 5 status finished reason completed, currency " +
					std::to_string(unsigned{hq_economy::armory_credits}) + " " + std::to_string(before) + " -> " + std::to_string(after) +
					(result == hq_payroll::outcome::granted ? " (settled)" : " (replayed, already settled this period)");
				return true;
			});
			if (ok && !notification.json.empty())
			{
				std::lock_guard lock{hq_payroll::notification_mutex};
				hq_payroll::notification = std::move(notification);
			}
			return ok;
		}

		// Timestamp plus parameters identifies a repeated native event. Zero timestamps
		// are not deduplicated because multiple genuine kills could otherwise collapse.
		std::string fingerprint = event.name + ":" + std::to_string(event.timestamp);
		for (const auto& parameter : event.parameters)
			fingerprint += ":" + parameter.selector + "=" + std::to_string(parameter.value);
		std::uint64_t hash = 14695981039346656037ULL;
		for (const auto byte : fingerprint) { hash ^= static_cast<unsigned char>(byte); hash *= 1099511628211ULL; }
		const auto key = "event:" + std::to_string(hash);
		return hq_economy::transact([&](hq_economy::state& data)
		{
			if (event.timestamp > 0 && data.transactions.contains(key)) return true;
			if (payroll)
			{
				auto& entry = data.achievements["payroll_officer"];
				// Local policy: 200 AC every four hours. Never replace an unclaimed reward.
				if (entry.status != "claimable" && (!entry.completion || now >= entry.completion + 4 * 3600))
				{
					entry = {};
					entry.name = "payroll_officer"; entry.challenge_name = entry.name;
					entry.kind = 5; entry.target = 1; entry.progress = 1;
					entry.activation = now; entry.offer_day = now / 86400;
					entry.status = "claimable";
					entry.rewards = {{"GRANT_CURRENCY", hq_economy::armory_credits, hq_economy::payroll_amount}};
				}
			}
			for (auto& [name, entry] : data.achievements)
			{
				if (entry.status != "inProgress") continue;
				// dwgamechallenges.csv event column and predicate, for the enabled catalog.
				// In particular weekly_ch_wins binds event 5 with no extra predicate.
				bool matches_event = (kills && (name == "daily_ch_kills" || name == "weekly_ch_kills")) ||
					(end_game && (name == "weekly_ch_wins" || name == "contract_mp_1")) ||
					(multi_kill && name == "contract_mp_3") || (streak && name == "weekly_ch_scorestreak_calls") ||
					(duel && name == "daily_ch_1v1_wins") || (social && name == "daily_ch_commend");
				if (kills && (name == "daily_ch_headshots" || name == "contract_mp_2"))
					for (const auto& parameter : event.parameters)
						matches_event |= parameter.selector == "6" && parameter.value == 1;
				if (!matches_event) continue;
				if (entry.progress < entry.target) ++entry.progress;
				if (entry.progress >= entry.target) entry.status = "claimable";
			}
			if (event.timestamp > 0) data.transactions[key] = std::to_string(now);
			// Event replay window is bounded independently of permanent claim receipts.
			std::size_t events{};
			for (const auto& [id, value] : data.transactions) if (id.starts_with("event:")) ++events;
			for (auto it = data.transactions.begin(); events > 2048 && it != data.transactions.end();)
			{
				if (it->first.starts_with("event:")) { it = data.transactions.erase(it); --events; }
				else ++it;
			}
			return true;
		});
	}

	std::string dispatch(const std::string_view body)
	{
		try
		{
			rapidjson::Document request{};
			if (body.size() > 64 * 1024) return R"({"Status":"error","reason":"request_too_large"})";
			request.Parse<rapidjson::kParseIterativeFlag>(body.data(), body.size());
			if (request.HasParseError() || !request.IsObject()) return R"({"Status":"error","reason":"invalid_json"})";
			const auto action = string(request, "Action");
			const auto client_tx = string(request, "ClientTx");
			rapidjson::Document response{rapidjson::kObjectType};
			auto& alloc = response.GetAllocator();
			response.AddMember("Version", 0, alloc);
			response.AddMember("Action", text(action, alloc), alloc);
			response.AddMember("Status", "ok", alloc);
			response.AddMember("ClientTx", text(client_tx, alloc), alloc);
			const auto fail = [&](const char* reason)
			{
				response["Status"].SetString("error", alloc);
				response.AddMember("reason", text(reason, alloc), alloc);
				return encode(response);
			};
			const auto now = static_cast<std::uint64_t>(time(nullptr));
			const auto day = now / 86400;
			auto scheduled = offers(day);
			hq_economy::state data{};
			bool economy_available = true;
			if (action != "pump_global_achievement_counters")
			{
				try { data = hq_economy::snapshot(); }
				catch (const std::exception& error)
				{
					economy_available = false;
					if (action != "get_user_achievements") throw;
					// A damaged HQ file must not hide independently persisted Zombies records.
					console::error("[HQ AE] HQ records unavailable: %s\n", error.what());
				}
			}
			const auto fetch = action == "get_user_achievements" || action == "get_scheduled_user_achievements" ||
				action == "get_expired_user_achievements" || action == "get_user_achievements_for_users";
			if (economy_available && (fetch || action.starts_with("activate_") || action == "deactivate_user_achievement"))
			{
				auto preview = data;
				if (reconcile_offers(preview, day))
				{
					if (!hq_economy::transact([&](auto& next) { reconcile_offers(next, day); return true; })) return fail("offer_save_failed");
					data = hq_economy::snapshot();
				}
			}
			std::erase_if(scheduled, [](const auto& entry) { return entry.kind == 1 || entry.kind == 2; });
			for (const auto& [name, entry] : data.achievements)
				if ((entry.kind == 1 || entry.kind == 2) && (entry.status == "available" || entry.status == "inProgress" || entry.status == "claimable")) scheduled.push_back(entry);
			if (action == "get_user_achievements_for_users")
			{
				if (request.HasMember("UserIDs") && !request["UserIDs"].IsArray()) return fail("invalid_user_ids");
				rapidjson::Value users{rapidjson::kObjectType};
				const auto local_id = std::to_string(steam::SteamUser()->GetSteamID().bits);
				const auto add = [&](const std::string& id)
				{
					if (users.HasMember(id.c_str())) return;
					rapidjson::Value entries{rapidjson::kArrayType};
					std::size_t limit = 1000;
					if (request.HasMember("Limit") && request["Limit"].IsUint() && request["Limit"].GetUint())
						limit = std::min<std::size_t>(1000, request["Limit"].GetUint());
					if (id == local_id) for (const auto& [name, entry] : data.achievements)
						if (entries.Size() < limit && matches(request, entry)) entries.PushBack(serialize(entry, alloc, day), alloc);
					users.AddMember(text(id, alloc), entries, alloc);
				};
				if (request.HasMember("UserIDs") && request["UserIDs"].IsArray())
					for (const auto& id : request["UserIDs"].GetArray())
					{
						if (id.IsString()) add(id.GetString());
						else if (id.IsUint64()) add(std::to_string(id.GetUint64()));
					}
				if (!request.HasMember("UserIDs")) add(std::to_string(steam::SteamUser()->GetSteamID().bits));
				response.AddMember("Achievements", users, alloc);
				response.AddMember("NextPageToken", "", alloc);
			}
			else if (action == "get_user_achievements" || action == "get_scheduled_user_achievements" ||
				action == "get_expired_user_achievements")
			{
				rapidjson::Value results{rapidjson::kArrayType};
				if (action == "get_user_achievements" && request.HasMember("UserIDs"))
				{
					if (!request["UserIDs"].IsArray()) return fail("invalid_user_ids");
					const auto local_id = steam::SteamUser()->GetSteamID().bits;
					bool local{};
					for (const auto& id : request["UserIDs"].GetArray())
						local |= (id.IsUint64() && id.GetUint64() == local_id) ||
							(id.IsString() && std::string_view{id.GetString()} == std::to_string(local_id));
					if (!local)
					{
						response.AddMember("Achievements", results, alloc);
						response.AddMember("NextPageToken", "", alloc);
						return encode(response);
					}
				}
				if (action == "get_user_achievements")
					{
					for (const auto& legacy : achievement_store::get_all())
					{
						hq_economy::achievement filter{};
						filter.name = legacy.name; filter.kind = legacy.kind;
						filter.status = get_achievement_status_name(legacy.status);
						if (matches(request, filter))
						{
							auto record = achievement_response::serialize_achievements({legacy}, alloc);
							results.PushBack(record[0], alloc);
						}
					}
					}
				std::vector<hq_economy::achievement> entries{};
				if (action == "get_scheduled_user_achievements")
				{
					for (auto entry : scheduled)
					{
						const auto it = data.achievements.find(entry.name);
						if (it != data.achievements.end() && (it->second.status == "inProgress" ||
							it->second.status == "claimable" || it->second.offer_day == entry.offer_day)) entry = it->second;
						entries.push_back(entry);
					}
					rapidjson::Value periods{rapidjson::kObjectType}, limits{rapidjson::kObjectType};
					periods.AddMember("1", period_end(1, day), alloc);
					periods.AddMember("2", period_end(2, day), alloc);
					periods.AddMember("4", period_end(4, day), alloc);
					limits.AddMember("1", 3, alloc);
					limits.AddMember("2", 3, alloc);
					limits.AddMember("4", 3, alloc);
					response.AddMember("NextPeriodStartTimes", periods, alloc);
					response.AddMember("ActivationLimits", limits, alloc);
				}
				else for (const auto& [name, entry] : data.achievements)
				{
					if ((entry.status == "expired") != (action == "get_expired_user_achievements")) continue;
					if (action == "get_expired_user_achievements" && request.HasMember("Timestamp"))
					{
						if (!request["Timestamp"].IsUint64()) return fail("invalid_timestamp");
						if (entry.completion <= request["Timestamp"].GetUint64()) continue;
					}
					entries.push_back(entry);
				}
				for (const auto& entry : entries) if (matches(request, entry)) results.PushBack(serialize(entry, alloc, day, action == "get_scheduled_user_achievements"), alloc);
				std::size_t offset{};
				const auto token = string(request, "PageToken");
				if (!token.empty())
				{
					const auto parsed = std::from_chars(token.data(), token.data() + token.size(), offset);
					if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) return fail("invalid_page_token");
				}
				// A missing or zero Limit means "no limit"; never reject the request for it.
				std::size_t limit = 1000;
				if (request.HasMember("Limit") && request["Limit"].IsUint() && request["Limit"].GetUint() > 0)
				{
					limit = std::min<std::size_t>(request["Limit"].GetUint(), 1000);
				}
				rapidjson::Value page{rapidjson::kArrayType};
				const auto end = std::min<std::size_t>(results.Size(), std::min<std::size_t>(offset, results.Size()) + limit);
				for (auto i = offset; i < end; ++i) page.PushBack(results[static_cast<rapidjson::SizeType>(i)], alloc);
				response.AddMember("Achievements", page, alloc);
				response.AddMember("NextPageToken", text(end < results.Size() ? std::to_string(end) : "", alloc), alloc);
			}
			else if (action == "open_supply_drop")
			{
				// Native 0x2B0850 sends column 4; 0x2AEEA0 counts column 5 item IDs.
				const auto drop = string(request, "SupplyDropID");
				// Item ids from mp/supplyDropTypes.csv column f5; sd_zombie_rare is the
				// Quartermaster's "ZM" vendor SKU, opened from the same local loot pool.
				const std::uint32_t drop_id = drop == "sd_mp" ? 1 : drop == "sd_mp_rare" ? 2 :
					drop == "sd_zombie_rare" ? 6 : 0;
				if (!drop_id) return fail("unsupported_supply_drop");
				if (client_tx.empty() || client_tx.size() > 128 || client_tx.find('\0') != std::string::npos)
					return fail("invalid_transaction");
				std::vector<std::uint32_t> pool;
				{
					std::lock_guard lock{catalog_mutex};
					pool = loot_items;
				}
				const auto key = "drop:" + client_tx;
				const auto ok = hq_economy::transact([&](hq_economy::state& next)
				{
					const auto previous = next.transactions.find(key);
					if (previous != next.transactions.end())
					{
						rapidjson::Document receipt;
						receipt.Parse<rapidjson::kParseIterativeFlag>(previous->second.c_str());
						if (receipt.HasParseError() || !receipt.IsObject() || string(receipt, "SupplyDropID") != drop ||
							!receipt.HasMember("GrantedItems") || !receipt["GrantedItems"].IsArray()) return false;
						response.CopyFrom(receipt, alloc);
					}
					else
					{
						if (pool.empty()) return false;
						auto owned = next.inventory.find({drop_id, 0});
						if (owned == next.inventory.end() || !owned->second.quantity ||
							(owned->second.expires && owned->second.expires <= now)) return false;
						--owned->second.quantity;
						owned->second.modified = static_cast<std::uint32_t>(now);
						// Local policy: three uniform collection-item rolls, with replacement.
						// This is not a reconstruction of retail odds or rare guarantees.
						std::mt19937_64 random{std::random_device{}()};
						std::uniform_int_distribution<std::size_t> roll{0, pool.size() - 1};
						rapidjson::Value items{rapidjson::kArrayType};
						for (int i = 0; i < 3; ++i)
						{
							const auto id = pool[roll(random)];
							if (!hq_economy::grant(next, {"GRANT_PRODUCT", id, 1})) return false;
							rapidjson::Value item{rapidjson::kObjectType};
							item.AddMember("id", id, alloc);
							items.PushBack(item, alloc);
						}
						response.AddMember("SupplyDropID", text(drop, alloc), alloc);
						response.AddMember("GrantedItems", items, alloc);
						response.AddMember("GrantedCurrencies", rapidjson::Value{rapidjson::kArrayType}, alloc);
						next.transactions[key] = encode(response);
					}
					// Absolute quantities, including zero for the consumed drop. On replay,
					// use today's quantities so an old receipt cannot rewind the UI cache.
					std::vector<std::uint32_t> ids{drop_id};
					for (const auto& item : response["GrantedItems"].GetArray())
					{
						if (!item.IsObject() || !item.HasMember("id") || !item["id"].IsUint()) return false;
						ids.push_back(item["id"].GetUint());
					}
					std::sort(ids.begin(), ids.end());
					ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
					rapidjson::Value inventory{rapidjson::kArrayType};
					for (const auto id : ids)
					{
						const auto found = next.inventory.find({id, 0});
						if (found == next.inventory.end()) return false;
						const auto& entry = found->second;
						rapidjson::Value item{rapidjson::kObjectType};
						item.AddMember("item_id", id, alloc);
						item.AddMember("item_quantity", entry.quantity, alloc);
						item.AddMember("collision_field", entry.collision, alloc);
						item.AddMember("expiry_duration", std::numeric_limits<std::int64_t>::max(), alloc);
						item.AddMember("mod_date_time", entry.modified, alloc);
						inventory.PushBack(item, alloc);
					}
					response.AddMember("DetailedInventory", inventory, alloc);
					return true;
				});
				if (!ok) return fail("drop_unavailable_transaction_conflict_or_save_failed");
			}
			else if (action == "pump_global_achievement_counters")
				response.AddMember("CounterValues", rapidjson::Value{rapidjson::kObjectType}, alloc);
			else if (action == "activate_scheduled_user_achievement" || action == "activate_user_contract" ||
				action == "deactivate_user_achievement" || action == "claim_achievement_reward")
			{
				const auto name = string(request, "AchievementName");
				if (name.empty() || name.size() > 256 || client_tx.size() > 256)
					return fail("invalid_achievement");
				int kind{};
				if (request.HasMember("AchievementKind"))
				{
					if (!request["AchievementKind"].IsInt()) return fail("invalid_kind");
					kind = request["AchievementKind"].GetInt();
				}
				else if (const auto it = data.achievements.find(name); it != data.achievements.end()) kind = it->second.kind;
				else for (const auto& offer : scheduled) if (offer.name == name) kind = offer.kind;
				if (action == "claim_achievement_reward" && client_tx.empty()) return fail("missing_transaction");
				hq_economy::achievement updated{};
				bool replay{};
				const auto ok = hq_economy::transact([&](hq_economy::state& next)
				{
					auto it = next.achievements.find(name);
					if (action.starts_with("activate_"))
					{
						if ((action == "activate_user_contract") != (kind == 4)) return false;
						if (it != next.achievements.end() && it->second.kind == kind &&
							(it->second.status == "inProgress" || it->second.status == "claimable"))
						{
							updated = it->second;
							return true;
						}
						if (it != next.achievements.end() && it->second.status != "available" &&
							(kind == 2 ? it->second.offer_day / 7 == day / 7 : it->second.offer_day == day)) return false;
						const auto offer = std::find_if(scheduled.begin(), scheduled.end(), [&](const auto& e) { return e.name == name && e.kind == kind; });
						if (offer == scheduled.end()) return false;
						const auto active = std::count_if(next.achievements.begin(), next.achievements.end(), [&](const auto& pair)
						{
							return pair.second.kind == kind && (pair.second.status == "inProgress" || pair.second.status == "claimable");
						});
						if (active >= 3) return false;
						updated = *offer;
						updated.claim_transaction.clear(); updated.completion = 0;
						updated.activation = now;
						updated.status = "inProgress";
						next.achievements[name] = updated;
						return true;
					}
					if (it == next.achievements.end() || it->second.kind != kind) return false;
					auto& entry = it->second;
					if (action == "deactivate_user_achievement")
					{
						if (entry.status != "inProgress" && entry.status != "claimable" && entry.status != "inactive") return false;
						if (entry.kind != 1 && entry.kind != 2 && entry.kind != 4) return false;
						entry.status = "available";
						entry.offer_day = day;
						entry.claim_transaction.clear(); entry.completion = 0;
						entry.progress = 0;
						entry.activation = 0;
						entry.usage = 0;
					}
					else
					{
						const auto transaction_key = "claim:" + client_tx;
						const auto fingerprint = name + ":" + std::to_string(entry.offer_day);
						const auto previous = next.transactions.find(transaction_key);
						if (previous != next.transactions.end() &&
							(previous->second != fingerprint || entry.claim_transaction != client_tx)) return false;
						if (entry.status == "finished" || (entry.status == "available" && entry.claim_transaction == client_tx)) replay = true;
						else
						{
							if (entry.status != "claimable" || entry.progress < entry.target) return false;
							for (const auto& reward : entry.rewards) if (!hq_economy::grant(next, reward)) return false;
							entry.status = "finished";
							entry.completion = now;
							entry.claim_transaction = client_tx;
							next.transactions[transaction_key] = fingerprint;
						}
					}
					updated = entry;
					return true;
				});
				if (!ok) return fail("achievement_transition_rejected_or_save_failed");
				rapidjson::Value entries{rapidjson::kArrayType};
				entries.PushBack(serialize(updated, alloc, day), alloc);
				response.AddMember("Achievements", entries, alloc);
				if (action == "claim_achievement_reward")
				{
					rapidjson::Value items{rapidjson::kArrayType}, currencies{rapidjson::kArrayType};
					for (const auto& reward : updated.rewards)
					{
						if ((replay && client_tx != updated.claim_transaction) || reward.type == "ACTIVATE_ACHIEVEMENT") continue;
						rapidjson::Value value{rapidjson::kObjectType};
						const auto item = reward.type == "GRANT_PRODUCT";
						value.AddMember(rapidjson::StringRef(item ? "guid" : "currencyID"), reward.id, alloc);
						value.AddMember(rapidjson::StringRef(item ? "quantity" : "amount"), reward.amount, alloc);
						(item ? items : currencies).PushBack(value, alloc);
					}
					response.AddMember("itemsReceived", items, alloc);
					response.AddMember("currenciesReceived", currencies, alloc);
					response.AddMember("transactionID", text(updated.claim_transaction, alloc), alloc);
				}
			}
			else
			{
				hq_protocol::trace("unsupported_ae_json", std::string{body});
				console::warn("[HQ AE] unsupported action '%s': %.*s\n", action.c_str(), static_cast<int>(std::min<std::size_t>(body.size(), 768)), body.data());
				return fail("unsupported_action");
			}
			return encode(response);
		}
		catch (const std::exception& error)
		{
			console::error("[HQ AE] request failed: %s\n", error.what());
			return R"({"Status":"error","reason":"internal_error"})";
		}
	}
}
