#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "component/command.hpp"
#include "component/console/console.hpp"
#include "component/scheduler.hpp"
#include "game/game.hpp"
#include "game/demonware/achievement_engine.hpp"
#include "game/demonware/hq_protocol.hpp"

#include <utils/hook.hpp>
#include <mutex>
#include <optional>

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

		utils::hook::detour fetch_user_hook;
		utils::hook::detour fetch_scheduled_hook;
		utils::hook::detour scheduled_success_hook;
		utils::hook::detour scheduled_failure_hook;
		std::atomic_bool accepting{};
		std::mutex pending_mutex{};

		struct update
		{
			std::string request;
			std::string transaction;
		};

		std::array<std::optional<update>, 2> pending{};

		std::byte* task_data(const std::size_t index)
		{
			return index == 0 ? game::AE_UserAchievementTaskData.get() : game::AE_ScheduledAchievementTaskData.get();
		}

		std::string task_transaction(const std::byte* task)
		{
			const auto* value = reinterpret_cast<const char*>(task + transaction_offset);
			const auto* end = std::find(value, value + transaction_size, '\0');
			return end == value + transaction_size ? std::string{} : std::string{value, end};
		}

		const std::byte* scheduled_cache(const unsigned int controller)
		{
			return game::AE_ScheduledChallengeCache.get() + controller * scheduled_cache_stride;
		}

		void queue_response(const std::size_t index)
		{
			if (!accepting.load()) return;
			try
			{
				auto* task = task_data(index);
				const auto* request = game::AE_GetResponseString(task);
				if (!request) return;
				const auto length = strnlen_s(request, request_capacity);
				if (!length || length == request_capacity) return;
				const auto transaction = task_transaction(task);
				if (transaction.empty()) return;

				rapidjson::Document json{};
				json.Parse<rapidjson::kParseIterativeFlag>(request, length);
				const auto* action = index == 0 ? "get_user_achievements" : "get_scheduled_user_achievements";
				if (json.HasParseError() || !json.IsObject() ||
					!json.HasMember("Action") || !json["Action"].IsString() ||
					std::string_view{json["Action"].GetString(), json["Action"].GetStringLength()} != action ||
					!json.HasMember("ClientTx") || !json["ClientTx"].IsString() ||
					std::string_view{json["ClientTx"].GetString(), json["ClientTx"].GetStringLength()} != transaction)
				{
					console::warn("[HQ AE injection] rejected invalid native request\n");
					return;
				}

				// Copy the actual engine request, preserving its filters, page and Tx.
				std::lock_guard lock{pending_mutex};
				pending[index] = update{{request, length}, transaction};
			}
			catch (const std::exception& error)
			{
				console::error("[HQ AE injection] queue failed: %s\n", error.what());
			}
		}

		void dispatch_responses()
		{
			if (!accepting.load()) return;
			std::array<std::optional<update>, 2> updates{};
			{
				std::lock_guard lock{pending_mutex};
				updates.swap(pending);
			}
			for (std::size_t index = 0; index < updates.size(); ++index)
			{
				if (!updates[index]) continue;
				try
				{
					const auto& entry = *updates[index];
					const auto* task = task_data(index);
					if (!*reinterpret_cast<const std::uint32_t*>(task + active_offset) ||
						task_transaction(task) != entry.transaction) continue;
					const auto response = demonware::achievement_engine::dispatch(entry.request);
					demonware::hq_protocol::trace("injected_ae_request", entry.request);
					demonware::hq_protocol::trace("injected_ae_response", response);
					// AE_ProcessResponse reads the JSON from whichever string object it is
					// given, resolves Action -> task type and looks the task up by
					// (group 0, controller, type) itself, so the proven user bridge
					// object is a valid carrier for every action.
					auto* bridge = game::AE_UserAchievementTaskData.get() + 0xF8;
					if (game::AE_SetResponseString(bridge, response.c_str()))
					{
						game::AE_ProcessResponse(0, bridge, 0);
						console::info("[HQ AE injection] dispatched %s Tx=%s (%zu bytes), group 0\n",
							index == 0 ? "user/active" : "scheduled", entry.transaction.c_str(), response.size());
					}
				}
				catch (const std::exception& error)
				{
					console::error("[HQ AE injection] dispatch failed: %s\n", error.what());
				}
			}
		}

		// The engine's own bdReward reply completes the native task on the next
		// Demonware pump; its success callback (0x13C220) sets the cache ready byte
		// and raises the LUI achievementEngine event that makes the menu read the
		// cache. The cache therefore has to be populated before this hook returns,
		// not on a later scheduler tick.
		void inject_now(const std::size_t index)
		{
			queue_response(index);
			dispatch_responses();
		}

		bool fetch_user_stub(const unsigned int controller, const char* page,
			const void* transaction, const unsigned int account)
		{
			const auto result = fetch_user_hook.invoke<bool>(controller, page, transaction, account);
			if (result && controller == 0) inject_now(0);
			return result;
		}

		bool fetch_scheduled_stub(const unsigned int controller, const void* transaction)
		{
			const auto result = fetch_scheduled_hook.invoke<bool>(controller, transaction);
			if (result && controller == 0) inject_now(1);
			return result;
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
			fetch_user_hook.create(game::AE_FetchUserAchievementsByPage, fetch_user_stub);
			fetch_scheduled_hook.create(game::AE_FetchScheduledChallenges, fetch_scheduled_stub);
			scheduled_success_hook.create(game::AE_ScheduledTaskSucceeded, scheduled_success_stub);
			scheduled_failure_hook.create(game::AE_ScheduledTaskFailed, scheduled_failure_stub);
			accepting = true;
			// Fallback for anything queued outside the hooks (none today).
			scheduler::loop(dispatch_responses, scheduler::pipeline::main, 50ms);
			command::add("aefetch", fetch_command);
			command::add("aecache", print_scheduled_cache);
		}

		void pre_destroy() override
		{
			accepting = false;
			std::lock_guard lock{pending_mutex};
			pending = {};
		}
	};
}

REGISTER_COMPONENT(achievement_injection::component)
