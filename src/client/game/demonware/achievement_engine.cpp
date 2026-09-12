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

		rapidjson::Value serialize(const hq_economy::achievement& entry, allocator& alloc)
		{
			rapidjson::Value value{rapidjson::kObjectType};
			value.AddMember("name", text(entry.name, alloc), alloc);
			value.AddMember("challengeName", text(entry.challenge_name, alloc), alloc);
			value.AddMember("kind", entry.kind, alloc);
			value.AddMember("status", text(entry.status, alloc), alloc);
			value.AddMember("requiresClaim", true, alloc);
			value.AddMember("progress", entry.progress, alloc);
			value.AddMember("progressTarget", entry.target, alloc);
			value.AddMember("globalProgressTarget", 0, alloc);
			value.AddMember("globalCounterID", 0, alloc);
			value.AddMember("fulfilledTimes", entry.completion ? 1 : 0, alloc);
			value.AddMember("completionCount", entry.completion ? 1 : 0, alloc);
			value.AddMember("completionTimestamp", entry.completion, alloc);
			value.AddMember("activationTimestamp", entry.activation, alloc);
			value.AddMember("eventEndTimestamp", entry.status == "available" ? (entry.offer_day + 1) * 86400 : 0, alloc);
			value.AddMember("usageTimeTarget", entry.usage_target, alloc);
			value.AddMember("usageTimeRemaining", entry.usage_target - std::min(entry.usage_target, entry.usage), alloc);
			rapidjson::Value rewards{rapidjson::kArrayType};
			for (const auto& reward : entry.rewards)
			{
				rapidjson::Value result{rapidjson::kObjectType};
				result.AddMember("type", text(reward.type, alloc), alloc);
				if (reward.type == "ACTIVATE_ACHIEVEMENT") result.AddMember("name", text(reward.achievement_name, alloc), alloc);
				result.AddMember(rapidjson::StringRef(reward.type == "GRANT_PRODUCT" ? "product" : "currencyID"), reward.id, alloc);
				result.AddMember(rapidjson::StringRef(reward.type == "GRANT_PRODUCT" ? "num_times" : "amount"), reward.amount, alloc);
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
			std::vector<hq_economy::achievement> result{}, daily{};
			for (auto entry : definitions)
			{
				entry.offer_day = day;
				if (entry.kind == 1) daily.push_back(entry);
				else result.push_back(entry);
			}
			for (std::size_t i = 0; i < std::min<std::size_t>(3, daily.size()); ++i)
			{
				auto entry = daily[(day + i) % daily.size()];
				entry.offer_day = day;
				result.push_back(entry);
			}
			return result;
		}
	}

	void set_catalog(std::vector<hq_economy::achievement> catalog)
	{
		std::lock_guard lock{catalog_mutex};
		definitions = std::move(catalog);
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
			if (action != "get_user_achievements_for_users" && action != "pump_global_achievement_counters")
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
				// Preserve the Zombies multi-user projection, without sharing wallets.
				rapidjson::Value users{rapidjson::kObjectType};
				const auto add = [&](const std::string& id)
				{
					if (!users.HasMember(id.c_str())) users.AddMember(text(id, alloc),
						achievement_response::serialize_achievements(achievement_store::get_all(), alloc), alloc);
				};
				if (request.HasMember("UserIDs") && request["UserIDs"].IsArray())
					for (const auto& id : request["UserIDs"].GetArray())
					{
						if (id.IsString()) add(id.GetString());
						else if (id.IsUint64()) add(std::to_string(id.GetUint64()));
					}
				if (users.ObjectEmpty()) add(std::to_string(steam::SteamUser()->GetSteamID().bits));
				response.AddMember("Achievements", users, alloc);
				response.AddMember("NextPageToken", "", alloc);
			}
			else if (action == "get_user_achievements" || action == "get_scheduled_user_achievements" ||
				action == "get_expired_user_achievements")
			{
				rapidjson::Value results{rapidjson::kArrayType};
				if (action == "get_user_achievements")
					results = achievement_response::serialize_achievements(achievement_store::get_all(), alloc);
				std::vector<hq_economy::achievement> entries{};
				if (action == "get_scheduled_user_achievements")
				{
					for (auto entry : scheduled)
					{
						const auto it = data.achievements.find(entry.name);
						if (it != data.achievements.end() && (it->second.status == "inProgress" ||
							it->second.status == "claimable" || it->second.offer_day == day)) entry = it->second;
						entries.push_back(entry);
					}
					rapidjson::Value periods{rapidjson::kObjectType}, limits{rapidjson::kObjectType};
					periods.AddMember("1", (day + 1) * 86400, alloc);
					periods.AddMember("4", (day + 1) * 86400, alloc);
					limits.AddMember("1", 3, alloc);
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
				for (const auto& entry : entries) if (matches(request, entry)) results.PushBack(serialize(entry, alloc), alloc);
				std::size_t offset{};
				const auto token = string(request, "PageToken");
				if (!token.empty())
				{
					const auto parsed = std::from_chars(token.data(), token.data() + token.size(), offset);
					if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) return fail("invalid_page_token");
				}
				unsigned limit = 100;
				if (request.HasMember("Limit"))
				{
					if (!request["Limit"].IsUint() || !request["Limit"].GetUint() || request["Limit"].GetUint() > 1000)
						return fail("invalid_limit");
					limit = request["Limit"].GetUint();
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
				if (name.empty() || name.size() > 256 || client_tx.size() > 256 || !request.HasMember("AchievementKind") || !request["AchievementKind"].IsInt())
					return fail("invalid_achievement");
				const auto kind = request["AchievementKind"].GetInt();
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
						if (it != next.achievements.end() && it->second.offer_day == day) return false;
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
						entry.status = "inactive";
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
