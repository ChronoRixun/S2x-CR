#include <std_include.hpp>
#include "achievement_engine.hpp"
#include "achievement_response.hpp"
#include "hq_protocol.hpp"
#include "hq_logging.hpp"
#include "hq_payroll.hpp"
#include "hq_marketplace.hpp"
#include "component/console/console.hpp"
#include "game/game.hpp"
#include "steam/steam.hpp"
#include <charconv>
#include <chrono>
#include <random>
#include <set>

namespace demonware::achievement_engine
{
	namespace
	{
		std::atomic_bool event_cache_dirty{};
		std::mutex cache_sink_mutex{};
		std::function<void(cache_update)> cache_sink{};
		std::mutex catalog_mutex{};
		std::vector<hq_economy::achievement> definitions{};
		std::vector<std::uint32_t> loot_items{}, zombies_loot_items{};
		std::map<std::uint32_t, std::uint32_t> loot_duplicate_credits;
		std::map<std::string, hq_event_predicate::rule> event_rules;
		using allocator = rapidjson::Document::AllocatorType;

		bool daily(const int kind) { return kind == 1 || kind == 8; }
		bool weekly(const int kind) { return kind == 2 || kind == 9; }
		bool contract(const int kind) { return kind == 4 || kind == 11; }
		bool order(const int kind) { return daily(kind) || weekly(kind); }
		const char* bonus_name(const int kind)
		{
			return kind == 8 ? "zm_above_beyond_daily" : kind == 9 ? "zm_above_beyond_weekly" :
				kind == 1 ? "above_beyond_daily" : "above_beyond_weekly";
		}


		rapidjson::Value text(const std::string_view value, allocator& alloc)
		{
			return rapidjson::Value{value.data(), static_cast<rapidjson::SizeType>(value.size()), alloc};
		}

		std::string string(const rapidjson::Value& value, const char* key)
		{
			return value.HasMember(key) && value[key].IsString()
				? std::string{value[key].GetString(), value[key].GetStringLength()} : std::string{};
		}

		const char* diagnostic_text(const std::string_view value, char (&buffer)[257]) noexcept
		{
			// Inspect at most 64 bytes; hex escaping bounds the diagnostic text to 256 bytes.
			auto* output = buffer;
			for (const unsigned char byte : value.substr(0, 64))
			{
				if (byte < 32 || byte > 126 || byte == '\'' || byte == '\\')
				{
					constexpr char hex[] = "0123456789ABCDEF";
					*output++ = '\\';
					*output++ = 'x';
					*output++ = hex[byte >> 4];
					*output++ = hex[byte & 15];
				}
				else *output++ = byte;
			}
			*output = '\0';
			return buffer;
		}

		std::string encode(const rapidjson::Value& value)
		{
			rapidjson::StringBuffer buffer{};
			rapidjson::Writer<rapidjson::StringBuffer> writer{buffer};
			value.Accept(writer);
			return {buffer.GetString(), buffer.GetSize()};
		}

		// Slice a validated page and return its exclusive end position.
		std::size_t apply_page(rapidjson::Value& results, const std::size_t offset, const std::size_t limit,
			allocator& alloc)
		{
			rapidjson::Value page{rapidjson::kArrayType};
			const auto end = std::min<std::size_t>(results.Size(), std::min<std::size_t>(offset, results.Size()) + limit);
			for (auto i = offset; i < end; ++i) page.PushBack(results[static_cast<rapidjson::SizeType>(i)], alloc);
			results = std::move(page);
			return end;
		}

		// Single-user fetches retain permissive page parsing here and the foreign-ID early return in dispatch.
		bool apply_page(const rapidjson::Value& request, rapidjson::Value& results,
			std::string& next_token, allocator& alloc)
		{
			std::size_t offset{};
			const auto token = string(request, "PageToken");
			if (!token.empty())
			{
				const auto parsed = std::from_chars(token.data(), token.data() + token.size(), offset);
				if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) return false;
			}
			// A missing or zero Limit uses the default 1000-record cap.
			std::size_t limit = 1000;
			if (request.HasMember("Limit") && request["Limit"].IsUint() && request["Limit"].GetUint() > 0)
			{
				limit = std::min<std::size_t>(request["Limit"].GetUint(), 1000);
			}
			const auto count = results.Size();
			const auto end = apply_page(results, offset, limit, alloc);
			if (end < count) next_token = std::to_string(end);
			return true;
		}

		bool load_hq_records_or_legacy_fallback(hq_economy::state& data)
		{
			// Missing storage is initialized; unreadable, damaged or locked storage uses local
			// legacy fallback under every requested ID; no other user's records are loaded.
			// All internal errors propagate. Diagnostics use the existing bounded throttle
			// table, whose eviction can allow a message before the throttle window expires.
			try
			{
				data = hq_economy::snapshot();
				return true;
			}
			catch (const hq_economy::store_unavailable& error)
			{
				try
				{
					if (hq_protocol::report_due(std::string{"ae/records_unavailable/"} + error.what()))
						hq_logging::safe_error("[HQ AE] HQ records unavailable: %s\n", error.what());
				}
				catch (...) {} // Diagnostics must not prevent the legacy fallback.
				return false;
			}
		}

		bool above_beyond(const hq_economy::achievement& entry)
		{
			return entry.kind == 5 && (entry.name == "above_beyond_daily" || entry.name == "above_beyond_weekly" ||
				entry.name == "zm_above_beyond_daily" || entry.name == "zm_above_beyond_weekly");
		}

		void publish_cache_update(cache_update update)
		{
			std::function<void(cache_update)> sink;
			{
				std::lock_guard lock{cache_sink_mutex};
				sink = cache_sink;
			}
			try
			{
				if (sink) sink(std::move(update));
			}
			catch (const std::exception& error)
			{
				// Persistence has succeeded; a UI scheduling failure must not turn the
				// accepted claim into an error reply or encourage another reward attempt.
				console::error("[HQ AE] cache notification failed: %s\n", error.what());
			}
		}

		bool redeemed_order(const hq_economy::achievement& entry)
		{
			return (order(entry.kind) || contract(entry.kind)) &&
				entry.status == "finished" && !entry.claim_transaction.empty();
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
			const auto status = above_beyond(entry) ? "in_progress" : scheduled ? (entry.status == "inProgress" ? "in_progress" :
				entry.status == "finished" ? "completed" : entry.status.c_str()) :
				(entry.status == "available" ? "inactive" : entry.status.c_str());
			value.AddMember("status", text(status, alloc), alloc);
			value.AddMember("requiresClaim", !above_beyond(entry), alloc);
			value.AddMember("progress", entry.progress, alloc);
			value.AddMember("progressTarget", entry.target, alloc);
			value.AddMember("globalProgressTarget", 0, alloc);
			value.AddMember("globalCounterID", 0, alloc);
			value.AddMember("fulfilledTimes", entry.completion ? 1 : 0, alloc);
			value.AddMember("completionCount", entry.completion ? 1 : 0, alloc);
			value.AddMember("completionTimestamp", entry.completion, alloc);
			value.AddMember("activationTimestamp", entry.activation, alloc);
			// "expirationTimestamp" only. The native record parser 0x13A570 (every AE reply
			// and every push goes through it: build/research/ghidra/decomp-payroll/13A605.c)
			// maps "expirationTimestamp" to record+0x18 - the field AE_GetScheduledChallenges
			// (0x121A00) and AE_GetPlayerActiveChallenges (0x121F40) publish as
			// expirationTimestamp - but maps "eventEndTimestamp" to record+0x30 divided by
			// 1000, and record+0x30 is the LAST COMPLETION TIME that
			// AE_GetPlayerAchievementInfo (0x1213B0) turns into "timeSinceLastCompletion".
			// Emitting it after "completionTimestamp" therefore overwrote the real completion
			// time with period_end/1000 (~1970), so the mail kiosk's
			// "14400 <= timeSinceLastCompletion" test always chose the collectable branch and
			// the four-hour countdown never appeared. No shipped consumer reads the key:
			// 0x13A570 and 0x13EC20 are its only two references in the image.
			// Zero selects Completion Time / active match time in the retail Contracts UI.
			const auto expires = contract(entry.kind) ? std::uint64_t{0} : period_end((entry.name == "above_beyond_weekly" || entry.name == "zm_above_beyond_weekly") ? 2 : entry.kind, day);
			value.AddMember("expirationTimestamp", expires, alloc);
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
							const auto status = above_beyond(entry) ? "in_progress" : scheduled ? (entry.status == "inProgress" ? "in_progress" :
								entry.status == "finished" ? "completed" : entry.status.c_str()) :
								(entry.status == "available" ? "inactive" : entry.status.c_str());
							found |= requested == status || (above_beyond(entry) && requested == entry.status);
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
			for (const auto kind : {1, 2, 4, 8, 9, 11})
			{
				std::vector<hq_economy::achievement> pool{};
				for (const auto& entry : definitions) if (entry.kind == kind) pool.push_back(entry);
				const auto period = weekly(kind) ? day / 7 : day;
				for (std::size_t i = 0; i < std::min<std::size_t>(contract(kind) ? 9 : daily(kind) ? 6 : 3, pool.size()); ++i)
				{
					auto entry = pool[(period + i) % pool.size()];
					entry.offer_day = weekly(kind) ? period * 7 : day;
					result.push_back(entry);
				}
			}
			return result;
		}
	}

	std::uint64_t period_end(const int kind, const std::uint64_t day)
	{
		return (weekly(kind) ? (day / 7 + 1) * 7 : day + 1) * 86400;
	}

	bool contract_eligible(const hq_economy::state& data, const std::string_view name, const std::uint64_t now)
	{
		const auto day = now / 86400;
		const auto scheduled = offers(day);
		const auto offer = std::find_if(scheduled.begin(), scheduled.end(), [&](const auto& e) { return contract(e.kind) && e.name == name; });
		if (offer == scheduled.end()) return false;
		const auto prior = data.achievements.find(std::string{name});
		// No same-day retries after completion or timeout, matching activation policy.
		if (prior != data.achievements.end() && (prior->second.status == "inProgress" ||
			prior->second.status == "claimable" || (prior->second.status != "available" && prior->second.offer_day == day))) return false;
		unsigned occupied{};
		for (const auto& [key, entry] : data.achievements)
			if (entry.kind == offer->kind && (entry.status == "inProgress" || entry.status == "claimable")) ++occupied;
		for (const auto& sku : hq_marketplace::vendor_skus)
		{
			if (!*sku.contract || name == sku.contract || std::none_of(scheduled.begin(), scheduled.end(),
				[&](const auto& e) { return e.kind == offer->kind && e.name == sku.contract; })) continue;
			const auto active = data.achievements.find(sku.contract);
			if (active != data.achievements.end() && (active->second.status == "inProgress" || active->second.status == "claimable")) continue;
			const auto token = data.inventory.find({hq_marketplace::granted_items(sku).front(), 0});
			if (token != data.inventory.end() && token->second.quantity && (!token->second.expires || token->second.expires > now)) ++occupied;
		}
		return occupied < 3;
	}

	bool advance_contract_time(hq_economy::achievement& entry, const std::uint32_t seconds)
	{
		if (!contract(entry.kind) || entry.status != "inProgress" || !entry.usage_target || !seconds) return false;
		entry.usage += std::min(seconds, entry.usage_target - std::min(entry.usage, entry.usage_target));
		if (entry.usage >= entry.usage_target)
		{ entry.status = "expired"; entry.expired_at = static_cast<std::uint64_t>(time(nullptr)); }
		return true;
	}

	bool advance_contract_time(hq_economy::state& data, const std::uint32_t seconds)
	{
		bool changed{};
		for (auto& [name, entry] : data.achievements) changed |= advance_contract_time(entry, seconds);
		return changed;
	}

	bool reconcile_offers(hq_economy::state& data, const std::uint64_t day)
	{
		std::lock_guard lock{catalog_mutex};
		bool changed{};
		const auto have_counters = std::any_of(definitions.begin(), definitions.end(),
			[](const auto& d) { return d.name == "daily_ch_assault_kills"; });
		constexpr auto recount_marker = "migration:above-beyond-recount-v1";
		if (!hq_economy::valid_receipt_key(recount_marker)) return false;
		const auto recount = have_counters && !data.transactions.contains(recount_marker);
		std::uint32_t daily_claims{}, weekly_claims{};
		// Count before offer reconciliation can replace an older definition. Completion,
		// not acceptance/offer day, identifies the period in which a carried order paid.
		if (recount) for (const auto& [name, entry] : data.achievements)
		{
			if (!redeemed_order(entry) || !entry.completion) continue;
			const auto completed_day = entry.completion / 86400;
			if (entry.kind == 1 && completed_day == day) daily_claims += daily_claims < 6;
			if (entry.kind == 2 && completed_day <= day && completed_day / 7 == day / 7) weekly_claims += weekly_claims < 3;
		}
		for (const auto kind : {4, 11})
		{
			const auto present = std::any_of(definitions.begin(), definitions.end(), [kind](const auto& a) { return a.kind == kind; });
			if (!present) continue; // Preserve the other mode's contracts and paid progress.
			changed |= std::erase_if(data.achievements, [&](const auto& pair)
			{
				return pair.second.kind == kind && std::none_of(definitions.begin(), definitions.end(),
					[&](const auto& d) { return d.kind == kind && d.name == pair.first && d.target == pair.second.target && d.usage_target == pair.second.usage_target; });
			}) != 0;
		}
		for (const auto kind : {1, 2, 8, 9})
		{
			std::vector<hq_economy::achievement> pool;
			for (const auto& entry : definitions) if (entry.kind == kind) pool.push_back(entry);
			if (pool.empty()) continue; // table loading has not completed
			const auto limit = daily(kind) ? 6u : 3u;
			const auto current = [&](const auto& entry) { return weekly(kind) ? entry.offer_day / 7 == day / 7 : entry.offer_day == day; };
			std::size_t live{};
			for (auto& [name, entry] : data.achievements)
			{
				if (entry.kind != kind) continue;
				const auto definition = std::find_if(pool.begin(), pool.end(), [&](const auto& d) { return d.name == name; });
				if (definition != pool.end() && !std::equal(entry.rewards.begin(), entry.rewards.end(), definition->rewards.begin(), definition->rewards.end(),
					[](const auto& a, const auto& b) { return a.type == b.type && a.id == b.id && a.amount == b.amount && a.achievement_name == b.achievement_name; }))
				{ entry.rewards = definition->rewards; changed = true; }
				if (entry.status == "inProgress" || entry.status == "claimable")
				{
					++live;
					// A carried order keeps progress/activation/reward and gets today's
					// offer date, so its UI expiration uses the current boundary.
					if (entry.offer_day != day) { entry.offer_day = day; changed = true; }
				}
				else if (entry.status == "finished" && current(entry)) ++live;
				else if (entry.status == "available" && !current(entry))
				{ entry.status = "expired"; entry.expired_at = static_cast<std::uint64_t>(time(nullptr)); changed = true; }
			}
			// Preserve current available offers, including the just-abandoned one.
			for (auto& [name, entry] : data.achievements)
			{
				if (entry.kind != kind || entry.status != "available") continue;
				if (live >= limit) { entry.status = "expired"; entry.expired_at = static_cast<std::uint64_t>(time(nullptr)); changed = true; continue; }
				++live;
				if (entry.offer_day != day) { entry.offer_day = day; changed = true; }
			}
			const auto period = weekly(kind) ? day / 7 : day;
			// Prefer unused definitions; if the small local pool is exhausted,
			// completed definitions may be offered again to maintain three slots.
			// The old receipt remains until activation, and cannot grant twice.
			for (const auto reuse_completed : {false})
				for (std::size_t i = 0; live < limit && i < pool.size(); ++i)
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
		for (const auto kind : {1, 2, 8, 9})
		{
			if (kind < 8 ? !have_counters : std::none_of(definitions.begin(), definitions.end(),
				[kind](const auto& entry) { return entry.kind == kind; })) continue;
			const auto name = bonus_name(kind);
			const auto period = daily(kind) ? day : day / 7 * 7;
			auto& counter = data.achievements[name];
			if (counter.name.empty() || counter.offer_day != period)
			{
				counter = {}; counter.name = counter.challenge_name = name; counter.kind = 5;
				counter.offer_day = period; counter.target = daily(kind) ? 6 : 3; counter.status = "inProgress";
				counter.rewards = {{"GRANT_PRODUCT", kind >= 8 ? 6u : daily(kind) ? 1u : 2u, 1}}; changed = true;
			}
			if (recount && kind < 8)
			{
				counter.progress = daily(kind) ? daily_claims : weekly_claims;
				// Recount is bookkeeping only: never replay an already-paid bonus.
				if (counter.progress >= counter.target) counter.status = "finished";
				changed = true;
			}
		}
		if (recount) data.transactions[recount_marker] = std::to_string(day);
		return changed;
	}

	void set_cache_update_sink(std::function<void(cache_update)> sink)
	{
		std::lock_guard lock{cache_sink_mutex};
		cache_sink = std::move(sink);
	}

	std::string counter_push(const hq_economy::achievement& counter)
	{
		if (!above_beyond(counter)) return {};
		rapidjson::Document push{rapidjson::kObjectType};
		auto& alloc = push.GetAllocator();
		auto record = serialize(counter, alloc, counter.offer_day);
		push.CopyFrom(record, alloc);
		push.AddMember("type", "CHALLENGE", alloc);
		push.AddMember("reason", text(counter.progress >= counter.target ? "completed" : "inProgress", alloc), alloc);
		// Keep in_progress/requiresClaim=false even at target so the bonus remains
		// queryable. The store already granted its drop; no grant trigger is replayed.
		push.AddMember("triggers", rapidjson::Value{rapidjson::kArrayType}, alloc);
		return encode(push);
	}

	void set_event_rules(std::map<std::string, hq_event_predicate::rule> rules)
	{
		// A bad asset row fails closed; neither it nor saved progress supplies code.
		std::erase_if(rules, [](const auto& pair)
		{
			return !pair.second.event_id || !hq_event_predicate::evaluate(pair.second.expression, {}).valid;
		});
		std::lock_guard lock{catalog_mutex};
		event_rules = std::move(rules);
	}

	void set_catalog(std::vector<hq_economy::achievement> catalog)
	{
		std::lock_guard lock{catalog_mutex};
		definitions = std::move(catalog);
	}

	void set_loot_catalog(std::vector<std::uint32_t> items, const bool zombies,
		std::map<std::uint32_t, std::uint32_t> duplicate_credits)
	{
		std::erase_if(items, [](const auto id) { return id <= 2 || id > INT32_MAX; });
		std::sort(items.begin(), items.end());
		items.erase(std::unique(items.begin(), items.end()), items.end());
		if (items.size() > 10000) items.clear();
		std::lock_guard lock{catalog_mutex};
		(zombies ? zombies_loot_items : loot_items) = std::move(items);
		if (!zombies) loot_duplicate_credits = std::move(duplicate_credits);
	}

	void retry_event_cache_refresh()
	{
		event_cache_dirty.store(true);
	}

	bool consume_event_cache_refresh()
	{
		return event_cache_dirty.exchange(false);
	}

	static bool apply_event(hq_economy::state& data, const reward_game_events::event& event,
		const bool native_payroll, hq_payroll::push& notification, bool& changed)
	{
		const auto event_type = hq_event_predicate::event_id(event.name);
		const auto payroll = event_type == 18;
		if (!event_type) return true;
		if (!valid_event(event, native_payroll)) return false;
		std::map<std::string, hq_event_predicate::rule> rules;
		{
			std::lock_guard lock{catalog_mutex};
			rules = event_rules;
		}
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

			return [&]()
			{
				const auto before = data.currencies.contains(hq_economy::armory_credits) ? data.currencies.at(hq_economy::armory_credits) : 0;
				const auto result = hq_payroll::settle(data, event.timestamp, now, master_prestige);
				if (result == hq_payroll::outcome::rejected) return false;
				// A batch from another period is acknowledged but describes no pickup the
				// kiosk is waiting on, so it must not animate a collection.
				if (result == hq_payroll::outcome::stale) return true;
				const auto after = data.currencies.contains(hq_economy::armory_credits) ? data.currencies.at(hq_economy::armory_credits) : 0;
				// Published on a replay as well: the currency grant stays once per period, but
				// the kiosk arms a 5 s "Unable to get payroll at this time" banner on every
				// click and only an achievementEngine CompletionUpdate for this achievement
				// cancels it, so a second pickup inside the period needs the event too.
				auto entry = hq_payroll::project(data.achievements.at("payroll_officer"));
				entry.challenge_name = entry.name;
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
				notification.summary = entry.name + " kind 5 status finished reason completed, currency " +
					std::to_string(unsigned{hq_economy::armory_credits}) + " " + std::to_string(before) + " -> " + std::to_string(after) +
					(result == hq_payroll::outcome::granted ? " (settled)" : " (replayed, already settled this period)");
				return true;
			}();
		}

		// Native timestamps have second resolution. Normalize aliases and fields
		// for retries; Zombies kills additionally retain their occurrence within a
		// batch so two identical kills in the same second both count.
		// Zero timestamps retain legacy arrival semantics.
		std::vector<std::pair<unsigned, std::uint64_t>> parameters;
		for (const auto& parameter : event.parameters)
		{
			unsigned selector{};
			std::from_chars(parameter.selector.data(), parameter.selector.data() + parameter.selector.size(), selector);
			parameters.emplace_back(selector, parameter.value);
		}
		std::sort(parameters.begin(), parameters.end());
		std::string fingerprint = std::to_string(event_type) + ":" + std::to_string(event.timestamp);
		for (const auto& [selector, value] : parameters)
			fingerprint += ":" + std::to_string(selector) + "=" + std::to_string(value);
		// Occurrence zero preserves existing receipts. The SDK generates a new
		// transaction ID on retry; that ID must not participate in this key.
		if (event_type == 34 && event.occurrence) fingerprint += ":occurrence=" + std::to_string(event.occurrence);
		std::uint64_t hash = 14695981039346656037ULL;
		for (const auto byte : fingerprint) { hash ^= static_cast<unsigned char>(byte); hash *= 1099511628211ULL; }
		const auto key = "event:" + std::to_string(hash);
		if (!hq_economy::valid_receipt_key(key)) return false;
		return [&]()
		{
			if (event.timestamp > 0 && data.transactions.contains(key)) return true;
			// A soldier level-up awards a Rare Supply Drop (mp/supplyDropTypes.csv sd_mp_rare,
			// item 2), as the end-of-match screen promises. The event carries no rank; the
			// receipt above keeps it once per event.
			if (event_type == 14 && !game::environment::is_zombies())
			{
				if (!hq_economy::grant(data, {"GRANT_PRODUCT", 2, 1})) return false;
				changed = true;
				console::info("[HQ AE] rank up: granted a Rare Supply Drop\n");
			}
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
				// Activation is in seconds, event timestamps are in microseconds.
				// Zero timestamps count on arrival because their occurrence time is unknown.
				if (event.timestamp > 0 && static_cast<std::uint64_t>(event.timestamp) / 1000000 < entry.activation) continue;
				const auto rule = rules.find(name);
				if (rule == rules.end() || rule->second.event_id != event_type ||
					!hq_event_predicate::evaluate(rule->second.expression, event).matches) continue;
				// One occurrence per matching event. Column 4 is a filter, not a count
				// selector; multi_kill (2) never also counts as killed_a_player (1).
				changed = true;
				if (entry.progress < entry.target) ++entry.progress;
				if (entry.progress >= entry.target) entry.status = "claimable";
			}
			if (event.timestamp > 0)
			{
				if (data.revision == UINT64_MAX) return false;
				// Events in a batch share a revision. Advance the persisted sequence
				// separately so eviction retains insertion order within the batch too.
				std::uint64_t sequence = data.revision;
				for (const auto& [id, value] : data.transactions)
				{
					if (!id.starts_with("event:") || !value.starts_with("sequence:")) continue;
					std::uint64_t prior{};
					const auto text = std::string_view{value}.substr(9);
					const auto parsed = std::from_chars(text.data(), text.data() + text.size(), prior);
					if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()) sequence = std::max(sequence, prior);
				}
				if (sequence == UINT64_MAX) return false;
				data.transactions[key] = "sequence:" + std::to_string(sequence + 1);
			}
			std::vector<std::pair<std::uint64_t, std::string>> events;
			for (const auto& [id, value] : data.transactions)
			{
				if (!id.starts_with("event:")) continue;
				// Legacy receipts have no insertion sequence: retire them before new ones.
				std::uint64_t sequence{};
				if (value.starts_with("sequence:"))
				{
					const auto text = std::string_view{value}.substr(9);
					const auto parsed = std::from_chars(text.data(), text.data() + text.size(), sequence);
					if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) sequence = 0;
				}
				events.emplace_back(sequence, id);
			}
			// Keep the newest 2,048 events independently of permanent economic receipts.
			std::sort(events.begin(), events.end());
			for (std::size_t i = 2048; i < events.size(); ++i)
				data.transactions.erase(events[i - 2048].second);
			return true;
		}();
	}

	bool valid_event(const reward_game_events::event& event, const bool native_payroll)
	{
		const auto type = hq_event_predicate::event_id(event.name);
		if (event.occurrence && (type != 34 || event.occurrence >= 100)) return false;
		return !type || (event.timestamp >= 0 && hq_event_predicate::evaluate({}, event).valid &&
			!(native_payroll && type == 18 && event.timestamp == 0));
	}

	bool submit_relay_events(std::vector<reward_game_events::event>& events)
	{
		std::erase_if(events, [](const auto& event) { return !valid_event(event, true); });
		if (events.empty()) return true;
		hq_payroll::push notification{};
		bool changed{};
		const auto ok = hq_economy::transact([&](hq_economy::state& data)
		{
			std::erase_if(events, [&](const auto& event)
			{
				// Rejection must discard this event's mutations, not its valid neighbors.
				auto candidate = data;
				auto next_notification = notification;
				bool next_changed{};
				if (!apply_event(candidate, event, true, next_notification, next_changed)) return true;
				changed |= next_changed;
				data = std::move(candidate);
				notification = std::move(next_notification);
				return false;
			});
			return true;
		});
		// Publish after commit so rolled-back progress never reaches native caches.
		// One dirty bit coalesces event batches and transactions until the main-thread poll.
		if (ok && changed) event_cache_dirty = true;
		if (ok && !notification.json.empty())
		{
			std::lock_guard lock{hq_payroll::notification_mutex};
			hq_payroll::notification = std::move(notification);
		}
		return ok;
	}

	bool submit_events(const std::vector<reward_game_events::event>& events, const bool native_payroll)
	{
		bool recognized{};
		for (const auto& event : events)
		{
			if (!hq_event_predicate::event_id(event.name)) continue;
			recognized = true;
			if (!valid_event(event, native_payroll)) return false;
		}
		if (!recognized) return true;
		hq_payroll::push notification{};
		bool changed{};
		const auto ok = hq_economy::transact([&](hq_economy::state& data)
		{
			for (const auto& event : events)
				if (!apply_event(data, event, native_payroll, notification, changed)) return false;
			return true;
		});
		// Publish after commit so rolled-back progress never reaches native caches.
		// One dirty bit coalesces event batches and transactions until the main-thread poll.
		if (ok && changed) event_cache_dirty = true;
		if (ok && !notification.json.empty())
		{
			std::lock_guard lock{hq_payroll::notification_mutex};
			hq_payroll::notification = std::move(notification);
		}
		return ok;
	}

	bool submit_event(const reward_game_events::event& event, const bool native_payroll)
	{
		return submit_events({event}, native_payroll);
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
			if (action == "open_supply_drop" && (client_tx.empty() || !hq_economy::valid_receipt_key("drop:" + client_tx)))
				return fail("invalid_transaction");
			if (action == "claim_achievement_reward")
			{
				if (client_tx.empty()) return fail("missing_transaction");
				if (!hq_economy::valid_receipt_key("claim:" + client_tx)) return fail("invalid_achievement");
			}
			std::size_t limit = 1000;
			if (action == "get_user_achievements_for_users")
			{
				if (request.HasMember("UserIDs") && !request["UserIDs"].IsArray()) return fail("invalid_user_ids");
				// Validate once, even for empty UserIDs. Missing/zero Limit uses the 1000-record default cap.
				if (request.HasMember("Limit"))
				{
					if (!request["Limit"].IsUint()) return fail("invalid_page_token");
					if (request["Limit"].GetUint()) limit = std::min<std::size_t>(request["Limit"].GetUint(), 1000);
				}
				// PageToken is ignored, including its type, as before multi-user pagination.
			}
			const auto now = static_cast<std::uint64_t>(time(nullptr));
			const auto day = now / 86400;
			auto scheduled = offers(day);
			hq_economy::state data{};
			bool economy_available = true;
			if (action == "get_user_achievements" || action == "get_user_achievements_for_users")
				economy_available = load_hq_records_or_legacy_fallback(data);
			else if (action != "pump_global_achievement_counters") data = hq_economy::snapshot();
			const auto fetch = action == "get_user_achievements" || action == "get_scheduled_user_achievements" ||
				action == "get_expired_user_achievements" || action == "get_user_achievements_for_users";
			if (economy_available && (fetch || action.starts_with("activate_") || action == "deactivate_user_achievement" || action == "claim_achievement_reward"))
			{
				auto preview = data;
				if (reconcile_offers(preview, day))
				{
					cache_update reset{true};
					if (!hq_economy::transact([&](auto& next)
					{
						const auto before = next.achievements;
						reconcile_offers(next, day);
						for (const auto& [name, counter] : next.achievements)
						{
							const auto old = before.find(name);
							if (above_beyond(counter) && old != before.end() && old->second.offer_day != counter.offer_day)
								reset.counters.push_back(counter);
						}
						return true;
					})) return fail("offer_save_failed");
					if (!reset.counters.empty()) publish_cache_update(std::move(reset));
					data = hq_economy::snapshot();
				}
			}
			unsigned scheduled_kinds{};
			for (const auto& entry : scheduled) scheduled_kinds |= 1u << entry.kind;
			std::erase_if(scheduled, [](const auto& entry) { return order(entry.kind); });
			for (const auto& [name, entry] : data.achievements)
				if (order(entry.kind) && (scheduled_kinds & (1u << entry.kind)) && (entry.status == "available" || entry.status == "inProgress" || entry.status == "claimable" ||
					(entry.status == "finished" && (weekly(entry.kind) ? entry.offer_day / 7 == day / 7 : entry.offer_day == day)))) scheduled.push_back(entry);
			std::set<std::string> legacy_names;
			const auto append_legacy = [&](rapidjson::Value& results)
			{
				for (const auto& legacy : achievement_store::get_all())
				{
					// Persisted legacy completions win name collisions with HQ records.
					if (!legacy_names.insert(legacy.name).second) continue;
					hq_economy::achievement filter{};
					filter.name = legacy.name; filter.kind = legacy.kind;
					filter.status = get_achievement_status_name(legacy.status);
					if (matches(request, filter))
					{
						auto record = achievement_response::serialize_achievements({legacy}, alloc);
						results.PushBack(record[0], alloc);
					}
				}
			};
			if (action == "get_user_achievements_for_users")
			{
				rapidjson::Value users{rapidjson::kObjectType};
				const auto local_id = std::to_string(steam::SteamUser()->GetSteamID().bits);
				const auto add = [&](const std::string& id)
				{
					if (users.HasMember(id.c_str())) return;
					rapidjson::Value entries{rapidjson::kArrayType};
					if (id == local_id || !economy_available)
					{
						// Fallback reads local legacy records separately for each requested ID.
						legacy_names.clear();
						append_legacy(entries);
						for (const auto& [name, stored] : data.achievements)
						{
							const auto entry = hq_payroll::project(stored);
							if (!legacy_names.contains(entry.name) && !redeemed_order(entry) && matches(request, entry))
								entries.PushBack(serialize(entry, alloc, day), alloc);
						}
						if (entries.Size() > limit)
						{
							// This response handler accumulates at most 30 accepted records (native image offset
							// 0x142660; build/research/ghidra/decomp-payroll/142660.c:93); saved multi-user requests
							// use Limit 50. By owner policy, return one capped page with no continuation.
							// Global across users and requests.
							static std::mutex warning_mutex;
							static auto next_warning = std::chrono::steady_clock::time_point::min();
							const std::lock_guard lock{warning_mutex};
							const auto warning_now = std::chrono::steady_clock::now();
							if (warning_now >= next_warning)
							{
								next_warning = warning_now + std::chrono::seconds{60};
								char diagnostic_buffer[257];
								hq_logging::safe_warn("[HQ] Multi-user achievements for %s truncated from %u to %zu records\n",
									diagnostic_text(id, diagnostic_buffer), entries.Size(), limit);
							}
							entries.Erase(entries.Begin() + limit, entries.End());
						}
					}
					users.AddMember(text(id, alloc), entries, alloc);
				};
				if (request.HasMember("UserIDs"))
					for (const auto& id : request["UserIDs"].GetArray())
					{
						if (id.IsString()) add(id.GetString());
						else if (id.IsUint64()) add(std::to_string(id.GetUint64()));
					}
				if (!request.HasMember("UserIDs")) add(local_id);
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
				if (action == "get_user_achievements") append_legacy(results);
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
				else for (const auto& [name, stored] : data.achievements)
				{
					const auto entry = hq_payroll::project(stored);
					if ((entry.status == "expired") != (action == "get_expired_user_achievements")) continue;
					if (action == "get_user_achievements" && (legacy_names.contains(entry.name) || redeemed_order(entry))) continue;
					if (action == "get_expired_user_achievements" && request.HasMember("Timestamp"))
					{
						if (!request["Timestamp"].IsUint64()) return fail("invalid_timestamp");
						if (entry.expired_at <= request["Timestamp"].GetUint64()) continue;
					}
					entries.push_back(entry);
				}
				for (const auto& entry : entries) if (matches(request, entry)) results.PushBack(serialize(entry, alloc, day, action == "get_scheduled_user_achievements"), alloc);
				std::string next_token;
				if (!apply_page(request, results, next_token, alloc)) return fail("invalid_page_token");
				response.AddMember("Achievements", results, alloc);
				response.AddMember("NextPageToken", text(next_token, alloc), alloc);
			}
			else if (action == "open_supply_drop")
			{
				// Native 0x2B0850 sends column 4; 0x2AEEA0 counts column 5 item IDs.
				const auto drop = string(request, "SupplyDropID");
				// Item ids from mp/supplyDropTypes.csv column f5; sd_zombie_rare is the
				// Quartermaster's "ZM" vendor SKU; its two-stage reveal needs two regular
				// items followed by three Zombies consumables.
				const std::uint32_t drop_id = drop == "sd_mp" ? 1 : drop == "sd_mp_rare" ? 2 :
					drop == "sd_zombie_rare" ? 6 : 0;
				if (!drop_id) return fail("unsupported_supply_drop");
				std::vector<std::uint32_t> pool, consumables;
				std::map<std::uint32_t, std::uint32_t> duplicate_credits;
				{
					std::lock_guard lock{catalog_mutex};
					pool = loot_items;
					duplicate_credits = loot_duplicate_credits;
					if (drop_id == 6) consumables = zombies_loot_items;
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
						if (pool.empty() || (drop_id == 6 && consumables.empty())) return false;
						// Do not consume a drop before the main-thread asset lookup is ready.
						// Missing values are not permission to guess a payout or lose a duplicate.
						for (const auto id : pool) if (!duplicate_credits.contains(id)) return false;
						auto owned = next.inventory.find({drop_id, 0});
						if (owned == next.inventory.end() || !owned->second.quantity ||
							(owned->second.expires && owned->second.expires <= now)) return false;
						--owned->second.quantity;
						owned->second.modified = static_cast<std::uint32_t>(now);
						// The native ZM reveal partitions rewards, flips two non-consumable
						// cards, then reveals exactly three consumables. Sending only the
						// consumables leaves an invalid GUID in the first stage's second slot.
						// Uniform rolls with replacement remain local policy, not retail odds.
						std::mt19937_64 random{std::random_device{}()};
						rapidjson::Value items{rapidjson::kArrayType};
						std::uint32_t credits{};
						const auto balance = next.currencies.find(hq_economy::armory_credits);
						const auto balance_before = balance == next.currencies.end() ? 0u : balance->second;
						const auto grant_rolls = [&](const auto& candidates, const unsigned count, const bool stackable)
						{
							std::uniform_int_distribution<std::size_t> roll{0, candidates.size() - 1};
							for (unsigned i = 0; i < count; ++i)
							{
								const auto id = candidates[roll(random)];
								const auto prior = next.inventory.find({id, 0});
								if (!stackable && prior != next.inventory.end() && hq_economy::live(prior->second, now))
								{
									const auto amount = duplicate_credits.at(id);
									// The native GrantedCurrencies decoder reads a signed delta.
									if (amount > INT32_MAX - credits) return false;
									credits += amount;
								}
								else
								{
									// grant() replaces expired/zero-quantity items with permanent units.
									if (!hq_economy::grant(next, {"GRANT_PRODUCT", id, 1})) return false;
								}
								rapidjson::Value item{rapidjson::kObjectType};
								item.AddMember("id", id, alloc);
								items.PushBack(item, alloc);
							}
							return true;
						};
						if (!grant_rolls(pool, drop_id == 6 ? 2 : 3, false) ||
							(drop_id == 6 && !grant_rolls(consumables, 3, true))) return false;
						if (credits && !hq_economy::grant(next, {"GRANT_CURRENCY", hq_economy::armory_credits, credits})) return false;
						rapidjson::Value currencies{rapidjson::kArrayType};
						if (credits)
						{
							rapidjson::Value currency{rapidjson::kObjectType};
							currency.AddMember("currency_id", hq_economy::armory_credits, alloc);
							currency.AddMember("balance_before", balance_before, alloc);
							currency.AddMember("balance_delta", credits, alloc);
							currencies.PushBack(currency, alloc);
						}
						response.AddMember("SupplyDropID", text(drop, alloc), alloc);
						response.AddMember("GrantedItems", items, alloc);
						response.AddMember("GrantedCurrencies", currencies, alloc);
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
			else if (action == "start_mission")
			{
				// The only member the client's handler reads. It stores the id in its session
				// record and echoes it back in end_mission; the match-end path only sends
				// end_mission when the id is non-zero, so a zero here costs us that message.
				// Nothing else interprets the value, so a process-lifetime counter is enough.
				static std::atomic<std::uint32_t> missions{};
				auto id = ++missions;
				if (!id) id = ++missions;
				response.AddMember("MissionInstanceId", id, alloc);
				console::debug("[HQ AE] start_mission -> MissionInstanceId %u\n", id);
			}
			else if (action == "end_mission" || action == "reset_missions")
			{
				// Acknowledge only. GrantedItems, DetailedInventory and GrantedCurrencies are
				// the match-end grant channel and are deliberately omitted, which leaves the
				// client handler completely inert. reset_missions has no handler at all.
				console::debug("[HQ AE] %s acknowledged\n", action.c_str());
			}
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
				cache_update claim_update{};
				const auto ok = hq_economy::transact([&](hq_economy::state& next)
				{
					auto it = next.achievements.find(name);
					if (action.starts_with("activate_"))
					{
						if ((action == "activate_user_contract") != contract(kind)) return false;
						if (it != next.achievements.end() && it->second.kind == kind &&
							(it->second.status == "inProgress" || it->second.status == "claimable"))
						{
							updated = it->second;
							return true;
						}
						if (contract(kind) && !contract_eligible(next, name, now)) return false;
						if (it != next.achievements.end() && it->second.status != "available" &&
							(weekly(kind) ? it->second.offer_day / 7 == day / 7 : it->second.offer_day == day)) return false;
						const auto offer = std::find_if(scheduled.begin(), scheduled.end(), [&](const auto& e) { return e.name == name && e.kind == kind; });
						if (offer == scheduled.end()) return false;
						const auto active = std::count_if(next.achievements.begin(), next.achievements.end(), [&](const auto& pair)
						{
							return pair.second.kind == kind && (pair.second.status == "inProgress" || pair.second.status == "claimable");
						});
						if (active >= 3) return false;
						if (contract(kind))
						{
							// The menu buys the cost item, then AE_ActivatePlayerChallenge consumes it.
							// Consume with activation in one transaction; retries above are free.
							const auto sku = std::find_if(std::begin(hq_marketplace::vendor_skus),
								std::end(hq_marketplace::vendor_skus), [&](const auto& value) { return name == value.contract; });
							if (sku == std::end(hq_marketplace::vendor_skus)) return false;
							const auto token = next.inventory.find({hq_marketplace::granted_items(*sku).front(), 0});
							if (token == next.inventory.end() || !token->second.quantity ||
								(token->second.expires && token->second.expires <= now)) return false;
							--token->second.quantity;
							token->second.modified = static_cast<std::uint32_t>(now);
						}
						updated = *offer;
						updated.claim_transaction.clear(); updated.completion = 0;
						if (next.revision == UINT64_MAX) return false;
						updated.activation = now;
						updated.activation_generation = next.revision + 1;
						updated.status = "inProgress";
						next.achievements[name] = updated;
						return true;
					}
					if (it == next.achievements.end() || it->second.kind != kind) return false;
					auto& entry = it->second;
					if (action == "deactivate_user_achievement")
					{
						if (entry.status != "inProgress" && entry.status != "claimable" && entry.status != "inactive") return false;
						if (!order(entry.kind) && !contract(entry.kind)) return false;
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
							if (order(entry.kind))
							{
								const auto counter = next.achievements.find(bonus_name(entry.kind));
								if (counter != next.achievements.end() && counter->second.status == "inProgress")
								{
									auto& bonus = counter->second;
									if (++bonus.progress >= bonus.target)
									{
										for (const auto& reward : bonus.rewards) if (!hq_economy::grant(next, reward)) return false;
										bonus.status = "finished"; bonus.completion = now; bonus.claim_transaction = client_tx;
									}
								}
							}
						}
					}
					updated = entry;
					if (action == "claim_achievement_reward" && !replay)
					{
						// Every claim re-reads the native user cache: the counter push goes
						// through the undocumented 139C10 status mapper and may be dropped.
						claim_update.fetch_user = true;
						if (order(entry.kind))
						{
							const auto bonus = next.achievements.find(bonus_name(entry.kind));
							if (bonus != next.achievements.end())
							{
								claim_update.counters.push_back(bonus->second);
								claim_update.push_counters = true;
							}
						}
					}
					return true;
				});
				if (!ok) return fail("achievement_transition_rejected_or_save_failed");
				if (action == "claim_achievement_reward" && !replay &&
					(order(updated.kind) || contract(updated.kind)))
					publish_cache_update(std::move(claim_update));
				rapidjson::Value entries{rapidjson::kArrayType};
				entries.PushBack(serialize(updated, alloc, day), alloc);
				if (action == "claim_achievement_reward" && order(updated.kind))
				{
					const auto state = hq_economy::snapshot();
					const auto bonus = state.achievements.find(bonus_name(updated.kind));
					if (bonus != state.achievements.end()) entries.PushBack(serialize(bonus->second, alloc, day), alloc);
				}
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
				char diagnostic_buffer[257];
				console::warn("[HQ AE] unsupported action '%s': unsupported_action\n", diagnostic_text(action, diagnostic_buffer));
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
