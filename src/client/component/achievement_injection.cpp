#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "component/command.hpp"
#include "component/console/console.hpp"
#include "component/scheduler.hpp"
#include "game/game.hpp"
#include "game/demonware/achievement_engine.hpp"
#include "game/demonware/hq_protocol.hpp"

#include <utils/hook.hpp>

namespace achievement_injection
{
	namespace
	{
		// See build/research/ae-internals.md and the Ghidra output under
		// build/research/ghidra/decomp-quick. Only controller 0 is supported by
		// the existing response bridge; +0xF8 is also the native task stride.
		constexpr std::ptrdiff_t active_offset = 0xC0;
		constexpr std::ptrdiff_t transaction_offset = 0xD0;
		constexpr std::size_t transaction_size = 25;
		constexpr std::size_t request_capacity = 0x1800;

		// Scheduled-challenge cache read by Engine.AE_GetScheduledChallenges (0x121A00):
		// 0x1908 bytes per controller, 100 records of 0x30 bytes, ready byte at +0x1900.
		// Record: +0 requiresClaim, +4 usageTimeTarget, +8 kind, +0xC ID (-1 = empty),
		// +0x10 progressTarget, +0x18 expirationTimestamp, +0x20 reward*, +0x28 status
		// (1 available, 2 in_progress, 3 claimable, 4 completed). Filled by the
		// get_scheduled_user_achievements handler (0x13EF20) called from AE_ProcessResponse.
		constexpr std::ptrdiff_t scheduled_cache_stride = 0x1908;
		constexpr std::ptrdiff_t scheduled_cache_ready = 0x1900;
		constexpr std::size_t scheduled_cache_records = 100;
		constexpr std::size_t scheduled_record_size = 0x30;

		utils::hook::detour submit_hook;
		utils::hook::detour scheduled_success_hook;
		utils::hook::detour scheduled_failure_hook;

		const std::byte* scheduled_cache(const unsigned int controller)
		{
			return game::AE_ScheduledChallengeCache.get() + controller * scheduled_cache_stride;
		}

		// 0x8397E0 submits every native task after its data and callbacks are installed.
		// Only AE task types have the string/transaction layout used below.
		void submit_stub(std::byte* task)
		{
			submit_hook.invoke<void>(task);
			const auto controller = *reinterpret_cast<const unsigned int*>(task + 4);
			const auto type = *reinterpret_cast<const unsigned int*>(task + 0xC);
			if (controller != 0) return;
			switch (type)
			{
			case 0x16: case 0x19: case 0x2C: case 0x30: case 0x72: case 0x7F:
			case 0x82: case 0x84: case 0x85: case 0x8D: case 0x8E: case 0x8F:
			case 0x99: case 0x9E: case 0xA0: break;
			default: return;
			}
			try
			{
				auto* data = *reinterpret_cast<std::byte**>(task + 0x28);
				if (!data || !*reinterpret_cast<const unsigned int*>(data + active_offset)) return;
				const auto* request = game::AE_GetResponseString(data);
				if (!request) return;
				const auto length = strnlen_s(request, request_capacity);
				if (!length || length == request_capacity) return;
				const std::string body{request, length};
				rapidjson::Document json{};
				json.Parse<rapidjson::kParseIterativeFlag>(body.data(), body.size());
				const auto* tx = reinterpret_cast<const char*>(data + transaction_offset);
				const auto tx_length = strnlen_s(tx, transaction_size);
				if (json.HasParseError() || !json.IsObject() || tx_length == transaction_size ||
					!json.HasMember("ClientTx") || !json["ClientTx"].IsString() ||
					std::string_view{json["ClientTx"].GetString(), json["ClientTx"].GetStringLength()} != std::string_view{tx, tx_length}) return;
				const auto response = demonware::achievement_engine::dispatch(body);
				demonware::hq_protocol::trace("injected_ae_request", body);
				demonware::hq_protocol::trace("injected_ae_response", response);
				auto* bridge = game::AE_UserAchievementTaskData.get() + 0xF8;
				if (game::AE_SetResponseString(bridge, response.c_str()))
					game::AE_ProcessResponse(controller, bridge, 0);
			}
			catch (const std::exception& error)
			{
				console::error("[HQ AE injection] dispatch failed: %s\n", error.what());
			}
		}

		std::size_t count_scheduled_records(const unsigned int controller)
		{
			std::size_t count{};
			const auto* cache = scheduled_cache(controller);
			for (std::size_t i = 0; i < scheduled_cache_records; ++i)
			{
				if (*reinterpret_cast<const std::int32_t*>(cache + i * scheduled_record_size + 0xC) != -1) ++count;
			}
			return count;
		}

		void scheduled_success_stub(void* task)
		{
			scheduled_success_hook.invoke<void>(task);
			console::info("[HQ AE injection] native scheduled task SUCCEEDED: %zu cached record(s), ready=%d\n",
				count_scheduled_records(0), static_cast<int>(*(scheduled_cache(0) + scheduled_cache_ready)));
		}

		void scheduled_failure_stub(void* task)
		{
			scheduled_failure_hook.invoke<void>(task);
			console::warn("[HQ AE injection] native scheduled task FAILED: %zu cached record(s), ready=%d\n",
				count_scheduled_records(0), static_cast<int>(*(scheduled_cache(0) + scheduled_cache_ready)));
		}

		void print_scheduled_cache()
		{
			const auto* cache = scheduled_cache(0);
			console::info("[HQ AE] scheduled cache controller 0: ready=%d, %zu record(s)\n",
				static_cast<int>(*(cache + scheduled_cache_ready)), count_scheduled_records(0));
			for (std::size_t i = 0; i < scheduled_cache_records; ++i)
			{
				const auto* record = cache + i * scheduled_record_size;
				const auto id = *reinterpret_cast<const std::int32_t*>(record + 0xC);
				if (id == -1) continue;
				console::info("  [%zu] id %d kind %d target %d status %d requiresClaim %d usageTimeTarget %d expires %llu reward %p\n",
					i, id, *reinterpret_cast<const std::int32_t*>(record + 8),
					*reinterpret_cast<const std::int32_t*>(record + 0x10),
					*reinterpret_cast<const std::int32_t*>(record + 0x28),
					static_cast<int>(*reinterpret_cast<const std::uint8_t*>(record)),
					*reinterpret_cast<const std::int32_t*>(record + 4),
					*reinterpret_cast<const std::uint64_t*>(record + 0x18),
					*reinterpret_cast<void* const*>(record + 0x20));
			}
		}

		// Issues the same engine fetch the Lua wrappers (0x121760 / 0x121D00) issue,
		// so the whole path can be exercised from the console without walking Headquarters.
		void fetch_command(const command::params& params)
		{
			const std::string_view what = params.size() > 1 ? params[1] : "";
			if (what != "scheduled" && what != "user")
			{
				console::info("Usage: aefetch <scheduled|user>\n");
				return;
			}
			char transaction[32]{};
			game::AE_GenerateTransactionId(transaction);
			const auto ok = what == "scheduled"
				? game::AE_FetchScheduledChallenges(0, transaction)
				: game::AE_FetchUserAchievements(0, transaction);
			console::info("[HQ AE] aefetch %.*s -> %s, Tx=%s\n", static_cast<int>(what.size()), what.data(),
				ok ? "issued" : "rejected", transaction);
		}
	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedicated() || game::environment::is_zombies()) return;
			submit_hook.create(0x8397E0_g, submit_stub);
			scheduled_success_hook.create(game::AE_ScheduledTaskSucceeded, scheduled_success_stub);
			scheduled_failure_hook.create(game::AE_ScheduledTaskFailed, scheduled_failure_stub);
			command::add("aefetch", fetch_command);
			command::add("aecache", print_scheduled_cache);
		}

	};
}

REGISTER_COMPONENT(achievement_injection::component)

