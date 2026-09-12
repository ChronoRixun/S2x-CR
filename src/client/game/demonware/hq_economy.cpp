#include <std_include.hpp>
#include "hq_economy.hpp"
#include "component/console/console.hpp"
#include <utils/io.hpp>

namespace demonware::hq_economy
{
	namespace
	{
		constexpr auto state_path = "players2/user/hq_economy.json";
		std::mutex state_mutex{};
		// Parsed copy of the store. snapshot() runs on the game's main thread from
		// the AE injection loop, so it must not touch the disk once loaded; the
		// cache is only refreshed by a successful transact() or by invalidate().
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

		std::string string(const rapidjson::Value& value, const char* key)
		{
			if (!value.IsObject() || !value.HasMember(key) || !value[key].IsString() || value[key].GetStringLength() > 1024)
				throw std::runtime_error(std::string{"invalid economy string: "} + key);
			return {value[key].GetString(), value[key].GetStringLength()};
		}

		state load()
		{
			state data{};
			if (!std::filesystem::exists(state_path)) return data;
			if (std::filesystem::file_size(state_path) > 16 * 1024 * 1024) throw std::runtime_error("economy file too large");
			std::string bytes{};
			if (!utils::io::read_file(state_path, &bytes)) throw std::runtime_error("cannot read economy");
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
				if (!entry.guid || !data.inventory.emplace(std::make_pair(entry.guid, entry.collision), entry).second)
					throw std::runtime_error("invalid or duplicate item");
			}
			for (const auto& value : document["achievements"].GetArray())
			{
				achievement entry{};
				entry.name = string(value, "name");
				entry.challenge_name = string(value, "challengeName");
				entry.kind = static_cast<int>(number(value, "kind", 13));
				entry.progress = static_cast<std::uint32_t>(number(value, "progress", UINT32_MAX));
				entry.target = static_cast<std::uint32_t>(number(value, "progressTarget", UINT32_MAX));
				entry.activation = number(value, "activationTimestamp");
				entry.completion = number(value, "completionTimestamp");
				entry.offer_day = number(value, "offerDay");
				entry.usage_target = static_cast<std::uint32_t>(number(value, "usageTimeTarget", UINT32_MAX));
				entry.usage = static_cast<std::uint32_t>(number(value, "usageTime", entry.usage_target));
				entry.status = string(value, "status");
				entry.claim_transaction = string(value, "claimTransaction");
				if (entry.name.empty() || !entry.kind || !entry.target ||
					(entry.status != "available" && entry.status != "inactive" && entry.status != "inProgress" &&
					entry.status != "claimable" && entry.status != "finished" && entry.status != "expired"))
					throw std::runtime_error("invalid achievement state");
				if (!value.HasMember("successRewards") || !value["successRewards"].IsArray() || value["successRewards"].Size() > 100)
					throw std::runtime_error("invalid achievement rewards");
				for (const auto& reward_value : value["successRewards"].GetArray())
				{
					reward result{};
					result.type = string(reward_value, "type");
					if (reward_value.HasMember("achievementName")) result.achievement_name = string(reward_value, "achievementName");
					result.id = static_cast<std::uint32_t>(number(reward_value, "id", UINT32_MAX));
					result.amount = static_cast<std::uint32_t>(number(reward_value, "amount", UINT32_MAX));
					entry.rewards.push_back(result);
				}
				if (!data.achievements.emplace(entry.name, entry).second) throw std::runtime_error("duplicate achievement");
			}
			for (const auto& value : document["transactions"].GetArray())
				if (!data.transactions.emplace(string(value, "id"), string(value, "request")).second)
					throw std::runtime_error("duplicate transaction");
			return data;
		}

		std::string encode(const state& data)
		{
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
				writer.EndObject();
			}
			writer.EndArray();
			writer.Key("achievements"); writer.StartArray();
			for (const auto& [name, entry] : data.achievements)
			{
				writer.StartObject();
				writer.Key("name"); writer.String(entry.name.c_str());
				writer.Key("challengeName"); writer.String(entry.challenge_name.c_str());
				writer.Key("kind"); writer.Int(entry.kind);
				writer.Key("progress"); writer.Uint(entry.progress);
				writer.Key("progressTarget"); writer.Uint(entry.target);
				writer.Key("activationTimestamp"); writer.Uint64(entry.activation);
				writer.Key("completionTimestamp"); writer.Uint64(entry.completion);
				writer.Key("offerDay"); writer.Uint64(entry.offer_day);
				writer.Key("usageTimeTarget"); writer.Uint(entry.usage_target);
				writer.Key("usageTime"); writer.Uint(entry.usage);
				writer.Key("status"); writer.String(entry.status.c_str());
				writer.Key("claimTransaction"); writer.String(entry.claim_transaction.c_str());
				writer.Key("successRewards"); writer.StartArray();
				for (const auto& result : entry.rewards)
				{
					writer.StartObject();
					writer.Key("type"); writer.String(result.type.c_str());
					writer.Key("achievementName"); writer.String(result.achievement_name.c_str());
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
				writer.Key("id"); writer.String(id.c_str());
				writer.Key("request"); writer.String(request.c_str());
				writer.EndObject();
			}
			writer.EndArray(); writer.EndObject();
			return {buffer.GetString(), buffer.GetSize()};
		}

		bool save(const state& data)
		{
			const auto bytes = encode(data);
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

	state snapshot()
	{
		std::lock_guard lock{state_mutex};
		if (!cached)
		{
			const file_lock disk_lock{};
			cached = load();
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
			auto next = cached ? *cached : load();
			if (!mutation(next) || next.revision == UINT64_MAX || next.inventory.size() > 10000 ||
				next.achievements.size() > 10000 || next.transactions.size() > 10000) return false;
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
			if (active >= 3) return false;
			target->second.status = "inProgress";
			target->second.activation = static_cast<std::uint64_t>(time(nullptr));
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
