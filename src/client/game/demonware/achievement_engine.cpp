#include <std_include.hpp>
#include "achievement_engine.hpp"
#include "achievement_response.hpp"
#include "hq_protocol.hpp"
#include "component/console/console.hpp"
#include "steam/steam.hpp"
#include <charconv>

namespace demonware::achievement_engine
{
	namespace
	{
		std::mutex catalog_mutex{};
		std::vector<hq_economy::achievement> definitions{};
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

		rapidjson::Value serialize(const hq_economy::achievement& entry, allocator& alloc, const bool scheduled = false)
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
			value.AddMember("expirationTimestamp", (entry.offer_day + (entry.kind == 2 ? 7 : 1)) * 86400, alloc);
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
						found |= std::string_view{value.GetString()} ==
							(std::strcmp(key, "AchievementNames") == 0 ? entry.name : entry.status);
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

	void set_catalog(std::vector<hq_economy::achievement> catalog)
	{
		std::lock_guard lock{catalog_mutex};
		definitions = std::move(catalog);
	}

	bool submit_event(const reward_game_events::event& event)
	{
		const auto kills = event.name == "1" || event.name == "killed_a_player";
		const auto payroll = event.name == "18" || event.name == "picked_up_payroll";
		if (!kills && !payroll) return true;
		const auto now = static_cast<std::uint64_t>(time(nullptr));
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
					entry.rewards = {{"GRANT_CURRENCY", 2, 200}};
				}
			}
			if (kills) for (auto& [name, entry] : data.achievements)
			{
				if (entry.status != "inProgress") continue;
				bool matches_event = name == "daily_ch_kills" || name == "weekly_ch_kills";
				if (name == "daily_ch_headshots")
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
			const auto scheduled = offers(day);
			hq_economy::state data{};
			if (action != "pump_global_achievement_counters")
			{
				try { data = hq_economy::snapshot(); }
				catch (const std::exception& error)
				{
					if (action != "get_user_achievements") throw;
					// A damaged HQ file must not hide independently persisted Zombies records.
					console::error("[HQ AE] HQ records unavailable: %s\n", error.what());
				}
			}
			if (action == "get_user_achievements_for_users")
			{
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
						if (entries.Size() < limit && matches(request, entry)) entries.PushBack(serialize(entry, alloc), alloc);
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
					periods.AddMember("1", (day + 1) * 86400, alloc);
					periods.AddMember("2", (day / 7 + 1) * 7 * 86400, alloc);
					periods.AddMember("4", (day + 1) * 86400, alloc);
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
				for (const auto& entry : entries) if (matches(request, entry)) results.PushBack(serialize(entry, alloc, action == "get_scheduled_user_achievements"), alloc);
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
						entry.progress = 0;
						entry.activation = 0;
						entry.usage = 0;
					}
					else
					{
						const auto transaction_key = "claim:" + client_tx;
						const auto fingerprint = name + ":" + std::to_string(entry.offer_day);
						const auto previous = next.transactions.find(transaction_key);
						if (previous != next.transactions.end() && previous->second != fingerprint) return false;
						if (entry.status == "finished") replay = true;
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
				entries.PushBack(serialize(updated, alloc), alloc);
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
