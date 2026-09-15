#include <std_include.hpp>
#include "hq_economy.hpp"
#include "component/console/console.hpp"
#include <utils/io.hpp>
#include <charconv>

namespace demonware::hq_economy
{
	namespace
	{
		constexpr auto state_path = "players2/user/hq_economy.json";
		std::mutex state_mutex{};
		// Parsed copy of the store. snapshot() runs on the game's main thread from
		// the AE injection loop, so it must not touch the disk once loaded; another
		// instance's saved changes are observed only after a successful transact()
		// or an explicit hqeconomy reload (invalidate()). The file lock prevents lost writes.
		std::optional<state> cached{};

		class file_lock
		{
		public:
			file_lock()
			{
				std::filesystem::create_directories("players2/user");
				handle_ = CreateFileA("players2/user/hq_economy.lock", GENERIC_READ | GENERIC_WRITE,
					0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (handle_ == INVALID_HANDLE_VALUE) throw std::runtime_error("economy is locked by another process");
			}
			~file_lock() { CloseHandle(handle_); }
			file_lock(const file_lock&) = delete;
			file_lock& operator=(const file_lock&) = delete;
		private:
			HANDLE handle_{INVALID_HANDLE_VALUE};
		};

		std::uint64_t number(const rapidjson::Value& value, const char* key, std::uint64_t maximum = UINT64_MAX)
		{
			if (!value.IsObject() || !value.HasMember(key) || !value[key].IsUint64() || value[key].GetUint64() > maximum)
				throw std::runtime_error(std::string{"invalid economy number: "} + key);
			return value[key].GetUint64();
		}

		std::string string(const rapidjson::Value& value, const char* key, const std::size_t maximum = 1024)
		{
			if (!value.IsObject() || !value.HasMember(key) || !value[key].IsString() || value[key].GetStringLength() > maximum)
				throw std::runtime_error(std::string{"invalid economy string: "} + key);
			std::string result{value[key].GetString(), value[key].GetStringLength()};
			if (maximum == identifier_limit && result.find('\0') != std::string::npos)
				throw std::runtime_error(std::string{"invalid economy identifier: "} + key);
			return result;
		}

		state decode(const std::string& bytes)
		{
			state data{};
			if (bytes.size() > 16 * 1024 * 1024) throw std::runtime_error("economy file too large");
			rapidjson::Document document{};
			document.Parse<rapidjson::kParseIterativeFlag>(bytes.data(), bytes.size());
			if (document.HasParseError() || !document.IsObject() || number(document, "schemaVersion") != 1)
				throw std::runtime_error("invalid economy schema; original preserved");
			data.revision = number(document, "revision");
			for (const auto* key : {"currencies", "inventory", "achievements", "transactions"})
				if (!document.HasMember(key) || !document[key].IsArray() || document[key].Size() > 10000)
					throw std::runtime_error("invalid economy collection");
			for (const auto& value : document["currencies"].GetArray())
			{
				const auto id = static_cast<std::uint8_t>(number(value, "currencyID", UINT8_MAX));
				if (!data.currencies.emplace(id, static_cast<std::uint32_t>(number(value, "amount", UINT32_MAX))).second)
					throw std::runtime_error("duplicate currency");
			}
			for (const auto& value : document["inventory"].GetArray())
			{
				item entry{};
				entry.guid = static_cast<std::uint32_t>(number(value, "guid", UINT32_MAX));
				entry.quantity = static_cast<std::uint32_t>(number(value, "quantity", UINT32_MAX));
				entry.collision = static_cast<std::uint16_t>(number(value, "collision", UINT16_MAX));
				entry.modified = static_cast<std::uint32_t>(number(value, "modified", UINT32_MAX));
				entry.expires = static_cast<std::uint32_t>(number(value, "expires", UINT32_MAX));
				if (value.HasMember("itemData"))
				{
					const auto& item_bytes = value["itemData"];
					if (!item_bytes.IsArray() || item_bytes.Size() > 64) throw std::runtime_error("invalid item data");
					for (const auto& byte : item_bytes.GetArray())
					{
						if (!byte.IsUint() || byte.GetUint() > 255) throw std::runtime_error("invalid item data byte");
						entry.metadata += static_cast<char>(byte.GetUint());
					}
				}
				if (!entry.guid || !data.inventory.emplace(std::make_pair(entry.guid, entry.collision), entry).second)
					throw std::runtime_error("invalid or duplicate item");
			}
			for (const auto& value : document["achievements"].GetArray())
			{
				achievement entry{};
				entry.name = string(value, "name", identifier_limit);
				entry.challenge_name = string(value, "challengeName", identifier_limit);
				entry.kind = static_cast<int>(number(value, "kind", 13));
				entry.progress = static_cast<std::uint32_t>(number(value, "progress", UINT32_MAX));
				entry.target = static_cast<std::uint32_t>(number(value, "progressTarget", UINT32_MAX));
				entry.activation = number(value, "activationTimestamp");
				entry.activation_generation = value.HasMember("activationGeneration") ? number(value, "activationGeneration") : 0;
				entry.completion = number(value, "completionTimestamp");
				// Legacy expirations have no recoverable time: treat them as already reported.
				entry.expired_at = value.HasMember("expiredTimestamp") ? number(value, "expiredTimestamp") : 0;
				entry.offer_day = number(value, "offerDay");
				entry.usage_target = static_cast<std::uint32_t>(number(value, "usageTimeTarget", UINT32_MAX));
				entry.usage = static_cast<std::uint32_t>(number(value, "usageTime", entry.usage_target));
				entry.status = string(value, "status", identifier_limit);
				entry.claim_transaction = string(value, "claimTransaction", identifier_limit);
				if (entry.name.empty() || !entry.kind || !entry.target ||
					(entry.status != "available" && entry.status != "inactive" && entry.status != "inProgress" &&
					entry.status != "claimable" && entry.status != "finished" && entry.status != "expired"))
					throw std::runtime_error("invalid achievement state");
				if (!value.HasMember("successRewards") || !value["successRewards"].IsArray() || value["successRewards"].Size() > 100)
					throw std::runtime_error("invalid achievement rewards");
				for (const auto& reward_value : value["successRewards"].GetArray())
				{
					reward result{};
					result.type = string(reward_value, "type", identifier_limit);
					if (reward_value.HasMember("achievementName")) result.achievement_name = string(reward_value, "achievementName", identifier_limit);
					result.id = static_cast<std::uint32_t>(number(reward_value, "id", UINT32_MAX));
					result.amount = static_cast<std::uint32_t>(number(reward_value, "amount", UINT32_MAX));
					entry.rewards.push_back(result);
				}
				if (!data.achievements.emplace(entry.name, entry).second) throw std::runtime_error("duplicate achievement");
			}
			for (const auto& value : document["transactions"].GetArray())
			{
				const auto id = string(value, "id", identifier_limit);
				if (!valid_receipt_key(id)) throw std::runtime_error("invalid transaction identifier");
				if (!data.transactions.emplace(id, string(value, "request")).second)
					throw std::runtime_error("duplicate transaction");
			}
			return data;
		}

		state load()
		{
			if (!std::filesystem::exists(state_path)) return {};
			if (std::filesystem::file_size(state_path) > 16 * 1024 * 1024) throw std::runtime_error("economy file too large");
			std::string bytes{};
			if (!utils::io::read_file(state_path, &bytes)) throw std::runtime_error("cannot read economy");
			return decode(bytes);
		}

		bool migrate_contracts(state& data)
		{
			constexpr auto marker = "migration:retail-contracts-v1";
			if (!valid_receipt_key(marker)) throw std::runtime_error("invalid migration receipt");
			if (data.transactions.contains(marker)) return false;
			// Run before the asset catalog is ready, so an early AE fetch cannot publish
			// the owner's synthetic Slice-9 completions into the native cache.
			for (const auto* name : {"contract_mp_1", "contract_mp_2", "contract_mp_3"})
			{
				const auto entry = data.achievements.find(name);
				if (entry != data.achievements.end() && entry->second.kind == 4) data.achievements.erase(entry);
			}
			for (const auto id : {0x5000001u, 0x5000002u, 0x5000003u})
				if (const auto item = data.inventory.find({id, 0}); item != data.inventory.end()) item->second.quantity = 0;
			data.transactions.emplace(marker, "retired placeholder progress and tokens; receipts retained");
			return true;
		}

		bool migrate_contract_tokens(state& data)
		{
			constexpr auto marker = "migration:retail-contract-tokens-v1";
			if (!valid_receipt_key(marker)) throw std::runtime_error("invalid migration receipt");
			if (data.transactions.contains(marker)) return false;
			// Slice 10 stores already carry retail-contracts-v1. Retire every collision
			// of its unknown StatsTable tokens without replaying purchases or claims.
			for (auto& [key, entry] : data.inventory)
				if (entry.guid >= 0x50F0001 && entry.guid <= 0x50F0009) entry.quantity = 0;
			data.transactions.emplace(marker, "retired unknown contract tokens; receipts retained");
			return true;
		}

		std::string encode(const state& data)
		{
			// Map keys are not separate JSON fields; validate them before comparisons too.
			for (const auto& [key, entry] : data.inventory)
				if (key != std::make_pair(entry.guid, entry.collision)) throw std::runtime_error("invalid inventory key");
			for (const auto& [name, entry] : data.achievements)
				if (name != entry.name) throw std::runtime_error("invalid achievement key");
			rapidjson::StringBuffer buffer{};
			rapidjson::Writer<rapidjson::StringBuffer> writer{buffer};
			writer.StartObject();
			writer.Key("schemaVersion"); writer.Uint(1);
			writer.Key("revision"); writer.Uint64(data.revision);
			writer.Key("currencies"); writer.StartArray();
			for (const auto& [id, amount] : data.currencies)
			{
				writer.StartObject();
				writer.Key("currencyID"); writer.Uint(id);
				writer.Key("amount"); writer.Uint(amount);
				writer.EndObject();
			}
			writer.EndArray();
			writer.Key("inventory"); writer.StartArray();
			for (const auto& [key, entry] : data.inventory)
			{
				writer.StartObject();
				writer.Key("guid"); writer.Uint(entry.guid);
				writer.Key("quantity"); writer.Uint(entry.quantity);
				writer.Key("collision"); writer.Uint(entry.collision);
				writer.Key("modified"); writer.Uint(entry.modified);
				writer.Key("expires"); writer.Uint(entry.expires);
				writer.Key("itemData"); writer.StartArray();
				for (const auto byte : entry.metadata) writer.Uint(static_cast<unsigned char>(byte));
				writer.EndArray();
				writer.EndObject();
			}
			writer.EndArray();
			writer.Key("achievements"); writer.StartArray();
			for (const auto& [name, entry] : data.achievements)
			{
				writer.StartObject();
				writer.Key("name"); writer.String(entry.name.data(), static_cast<rapidjson::SizeType>(entry.name.size()));
				writer.Key("challengeName"); writer.String(entry.challenge_name.data(), static_cast<rapidjson::SizeType>(entry.challenge_name.size()));
				writer.Key("kind"); writer.Int(entry.kind);
				writer.Key("progress"); writer.Uint(entry.progress);
				writer.Key("progressTarget"); writer.Uint(entry.target);
				writer.Key("activationTimestamp"); writer.Uint64(entry.activation);
				writer.Key("activationGeneration"); writer.Uint64(entry.activation_generation);
				writer.Key("completionTimestamp"); writer.Uint64(entry.completion);
				writer.Key("expiredTimestamp"); writer.Uint64(entry.expired_at);
				writer.Key("offerDay"); writer.Uint64(entry.offer_day);
				writer.Key("usageTimeTarget"); writer.Uint(entry.usage_target);
				writer.Key("usageTime"); writer.Uint(entry.usage);
				writer.Key("status"); writer.String(entry.status.data(), static_cast<rapidjson::SizeType>(entry.status.size()));
				writer.Key("claimTransaction"); writer.String(entry.claim_transaction.data(), static_cast<rapidjson::SizeType>(entry.claim_transaction.size()));
				writer.Key("successRewards"); writer.StartArray();
				for (const auto& result : entry.rewards)
				{
					writer.StartObject();
					writer.Key("type"); writer.String(result.type.data(), static_cast<rapidjson::SizeType>(result.type.size()));
					writer.Key("achievementName"); writer.String(result.achievement_name.data(), static_cast<rapidjson::SizeType>(result.achievement_name.size()));
					writer.Key("id"); writer.Uint(result.id);
					writer.Key("amount"); writer.Uint(result.amount);
					writer.EndObject();
				}
				writer.EndArray(); writer.EndObject();
			}
			writer.EndArray();
			writer.Key("transactions"); writer.StartArray();
			for (const auto& [id, request] : data.transactions)
			{
				writer.StartObject();
				writer.Key("id"); writer.String(id.data(), static_cast<rapidjson::SizeType>(id.size()));
				writer.Key("request"); writer.String(request.data(), static_cast<rapidjson::SizeType>(request.size()));
				writer.EndObject();
			}
			writer.EndArray(); writer.EndObject();
			return {buffer.GetString(), buffer.GetSize()};
		}

		bool save(const state& data)
		{
			// Permanent receipts are never pruned: the lifetime ledger ceiling blocks
			// new economic receipts once full. Warn once per session, under state_mutex.
			if (data.transactions.size() > 10000)
			{
				static bool warned{};
				if (!std::exchange(warned, true))
					console::warn("[HQ economy] Receipt limit (10000) reached in players2/user/hq_economy.json; new receipts cannot be saved. Deleting players2/user/hq_economy.json resets only the Headquarters economy.\n");
				return false;
			}
			const auto bytes = encode(data);
			// Use the loader itself so every saved field and limit stays loadable.
			decode(bytes);
			const auto temporary = std::string{state_path} + ".tmp";
			const auto file = CreateFileA(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
			if (file == INVALID_HANDLE_VALUE) return false;
			DWORD written{};
			const auto ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
				written == bytes.size() && FlushFileBuffers(file);
			CloseHandle(file);
			return ok && MoveFileExA(temporary.c_str(), state_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
		}
	}

	bool valid_receipt_key(const std::string_view key)
	{
		return !key.empty() && key.size() <= identifier_limit && key.find('\0') == std::string_view::npos;
	}

	bool migrate_payroll(state& data)
	{
		// v1 parked the payroll balance in currency 7 (Social Score). A new stamp lets the
		// corrected pass run exactly once more on a store the old marker already touched.
		constexpr auto marker = "migration:payroll-currency6-v1";
		if (!valid_receipt_key(marker)) throw std::runtime_error("invalid migration receipt");
		if (data.transactions.contains(marker)) return false;
		// Legacy native receipts contain a microsecond timestamp; manual claim
		// receipts contain payroll_officer:<day>. Native acknowledgement of a
		// manual claim is not another payment: conservatively take max per day.
		std::map<std::uint64_t, std::pair<unsigned, unsigned>> days;
		const auto parse = [](const std::string_view text, std::uint64_t& value)
		{
			const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
			return !text.empty() && result.ec == std::errc{} && result.ptr == text.data() + text.size();
		};
		for (const auto& [id, request] : data.transactions)
		{
			std::uint64_t period{}, timestamp{};
			if (id.starts_with("payroll:") && parse(std::string_view{id}.substr(8), period) &&
				parse(request, timestamp) && timestamp && timestamp / 1000000 / 14400 == period)
				++days[timestamp / 1000000 / 86400].first;
			if (id.starts_with("claim:") && request.starts_with("payroll_officer:") &&
				parse(std::string_view{request}.substr(16), period)) ++days[period].second;
		}
		std::uint64_t accounted{};
		for (const auto& [day, counts] : days) accounted += std::max(counts.first, counts.second) * std::uint64_t{payroll_amount};
		// Drain the Social Score parking slot first, then any CP this client never moved.
		// The receipt total is the ceiling for BOTH legacy rows together, so a store that
		// already ran v1 cannot be credited twice for the same payroll.
		std::uint32_t moved{};
		for (const auto legacy : legacy_credit_currencies)
		{
			if (legacy == armory_credits || accounted <= moved) continue;
			const auto source = data.currencies.find(legacy);
			if (source == data.currencies.end() || !source->second) continue;
			auto& balance = data.currencies[armory_credits];
			const auto take = static_cast<std::uint32_t>(std::min({accounted - moved,
				std::uint64_t{source->second}, std::uint64_t{UINT32_MAX - balance}}));
			if (!take) continue;
			source->second -= take;
			balance += take;
			moved += take;
		}
		// Persisted local AC reward definitions must also stop issuing CP or Social Score.
		for (auto& [name, entry] : data.achievements)
			if (name == "payroll_officer" || name.starts_with("daily_ch_") || name.starts_with("weekly_ch_") || name.starts_with("contract_"))
				for (auto& reward : entry.rewards)
					if (reward.type == "GRANT_CURRENCY" && std::ranges::find(legacy_credit_currencies,
						reward.id) != std::end(legacy_credit_currencies)) reward.id = armory_credits;
		data.transactions.emplace(marker, std::to_string(moved));
		return true;
	}

	state snapshot()
	{
		std::lock_guard lock{state_mutex};
		if (!cached)
		{
			const file_lock disk_lock{};
			auto next = load();
			const auto payroll_changed = migrate_payroll(next);
			const auto tokens_changed = migrate_contract_tokens(next);
			if (migrate_contracts(next) || payroll_changed || tokens_changed)
			{
				if (next.revision == UINT64_MAX) throw std::runtime_error("economy revision overflow");
				++next.revision;
				if (!save(next)) throw std::runtime_error("payroll migration save failed");
			}
			cached = std::move(next);
		}
		return *cached;
	}

	void invalidate()
	{
		std::lock_guard lock{state_mutex};
		cached.reset();
	}

	bool transact(const std::function<bool(state&)>& mutation)
	{
		try
		{
			std::lock_guard lock{state_mutex};
			const file_lock disk_lock{};
			auto next = load(); // always validate the on-disk copy before mutating it
			const auto before = encode(next);
			migrate_payroll(next);
			migrate_contracts(next);
			migrate_contract_tokens(next);
			if (!mutation(next)) return false;
			if (encode(next) == before)
			{
				cached = std::move(next);
				return true;
			}
			if (next.revision == UINT64_MAX) return false;
			++next.revision;
			if (!save(next)) throw std::runtime_error("atomic economy save failed");
			cached = std::move(next);
			return true;
		}
		catch (const std::exception& error)
		{
			console::error("[HQ economy] %s\n", error.what());
			return false;
		}
	}

	bool grant(state& data, const reward& value)
	{
		if (value.type == "ACTIVATE_ACHIEVEMENT")
		{
			const auto target = data.achievements.find(value.achievement_name);
			if (target == data.achievements.end() || target->second.status != "inactive") return false;
			const auto active = std::count_if(data.achievements.begin(), data.achievements.end(), [&](const auto& pair)
			{
				return pair.second.kind == target->second.kind &&
					(pair.second.status == "inProgress" || pair.second.status == "claimable");
			});
			if (active >= 3 || data.revision == UINT64_MAX) return false;
			target->second.status = "inProgress";
			target->second.activation = static_cast<std::uint64_t>(time(nullptr));
			target->second.activation_generation = data.revision + 1;
			return true;
		}
		if (value.type == "GRANT_CURRENCY" || value.type == "SET_CURRENCY_BALANCE")
		{
			if (value.id > UINT8_MAX) return false;
			auto& balance = data.currencies[static_cast<std::uint8_t>(value.id)];
			if (value.type == "SET_CURRENCY_BALANCE") balance = value.amount;
			else
			{
				if (value.amount > UINT32_MAX - balance) return false;
				balance += value.amount;
			}
			return true;
		}
		// Only resolved numeric item GUIDs are accepted; product bundles need a catalog.
		if (value.type == "GRANT_PRODUCT" && value.id && value.amount)
		{
			auto& entry = data.inventory[{value.id, 0}];
			if (value.amount > UINT32_MAX - entry.quantity) return false;
			entry.guid = value.id;
			entry.quantity += value.amount;
			entry.modified = static_cast<std::uint32_t>(time(nullptr));
			return true;
		}
		return false;
	}
}
