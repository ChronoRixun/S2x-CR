#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "component/command.hpp"
#include "component/console/console.hpp"
#include "component/scheduler.hpp"
#include "game/game.hpp"
#include "game/demonware/achievement_engine.hpp"
#include "game/demonware/hq_protocol.hpp"

#include <utils/finally.hpp>
#include <utils/hook.hpp>

namespace achievement_injection
{
	namespace
	{
		// See build/research/ae-internals.md and the Ghidra output under
		// build/research/ghidra/decomp-quick. Only controller 0 is supported by
		// the existing response bridge; +0xF8 is also the native task stride.
		//
		// Every Achievement Engine task data blob shares one 0xF8-byte struct: all six
		// documented per-task bases (0x60391D0 scheduled, 0x6039A60 user, 0x603A030
		// activate, 0x60393C0 deactivate, 0x60395B0 claim, 0x6039C50 expired) index their
		// controller with the same 0xF8 stride, so the active flag at +0xC0 and the
		// transaction at +0xD0 are valid for every AE type, not just the two fetches that
		// were confirmed by decompile. Nothing below may read at or beyond +0xF8.
		constexpr std::ptrdiff_t active_offset = 0xC0;
		constexpr std::ptrdiff_t transaction_offset = 0xD0;
		constexpr std::size_t transaction_size = 25;
		constexpr std::size_t task_data_size = 0xF8;
		constexpr std::size_t request_capacity = 0x1800;
		static_assert(static_cast<std::size_t>(active_offset) + sizeof(unsigned int) <= task_data_size);
		static_assert(static_cast<std::size_t>(transaction_offset) + transaction_size <= task_data_size);

		// Native task table for group 0, as walked by the task lookup 0x208270: entries
		// start at table + 8 with a stride of 0x50 and there are 32 of them. The shared
		// submission function 0x8397E0 has ~90 call sites across unrelated subsystems
		// (build/research/ghidra/slice2-refs.txt), so a foreign task whose type happens to
		// equal one of the AE type constants must not have its data blob read as AE data.
		constexpr std::ptrdiff_t task_table_first_entry = 8;
		constexpr std::ptrdiff_t task_table_stride = 0x50;
		constexpr std::size_t task_table_entries = 32;

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

		// Guards the synchronous AE_ProcessResponse call below against re-entering this hook.
		thread_local bool in_dispatch = false;

		utils::hook::detour submit_hook;
		utils::hook::detour scheduled_success_hook;
		utils::hook::detour scheduled_failure_hook;

		const std::byte* scheduled_cache(const unsigned int controller)
		{
			return game::AE_ScheduledChallengeCache.get() + controller * scheduled_cache_stride;
		}

		// True only when the task pointer is an entry of the group-0 native task table.
		bool is_group_zero_task(const std::byte* task)
		{
			const auto* table = game::AE_TaskGroupTables.get()[0];
			if (!table) return false;
			for (std::size_t i = 0; i < task_table_entries; ++i)
			{
				if (task == table + task_table_first_entry + i * task_table_stride) return true;
			}
			return false;
		}

		// Every AE type constant below is < 0x100, so one flag per type value is enough.
		void warn_rejected_once(const unsigned int type)
		{
			static std::atomic<bool> warned[256]{};
			if (type >= std::size(warned) || warned[type].exchange(true)) return;
			console::warn("[HQ AE injection] task type 0x%X rejected: not an entry of the group 0 task table\n", type);
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
			// The type alone does not identify an AE task: only tasks that live in the
			// group 0 table are ones the Achievement Engine registered.
			if (!is_group_zero_task(task))
			{
				warn_rejected_once(type);
				return;
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
				// Bail on an empty transaction id as well as an unterminated one, so an
				// uninitialised request is never answered.
				if (json.HasParseError() || !json.IsObject() || !tx_length || tx_length == transaction_size ||
					!json.HasMember("ClientTx") || !json["ClientTx"].IsString() ||
					std::string_view{json["ClientTx"].GetString(), json["ClientTx"].GetStringLength()} != std::string_view{tx, tx_length}) return;
				// AE_ProcessResponse runs native handlers that can submit further tasks
				// through 0x8397E0; a shallow guard keeps that from recursing into this hook.
				if (in_dispatch)
				{
					console::warn("[HQ AE injection] re-entrant dispatch for task type 0x%X skipped\n", type);
					return;
				}
				in_dispatch = true;
				const auto reset = utils::finally([]
				{
					in_dispatch = false;
				});
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

		void refresh_user_cache()
		{
			char transaction[32]{};
			game::AE_GenerateTransactionId(transaction);
			const auto ok = game::AE_FetchUserAchievements(0, transaction);
			console::info("[HQ AE] counter cache fetch %s, Tx=%s\n", ok ? "issued" : "rejected", transaction);
		}

		std::atomic_bool user_fetch_queued{};

		// One-shot coalesced version of the `aefetch user` console command: a second claim
		// while a fetch is still pending folds into it instead of queueing a duplicate.
		void schedule_user_cache_fetch()
		{
			if (user_fetch_queued.exchange(true)) return;
			scheduler::once([]
			{
				user_fetch_queued = false;
				refresh_user_cache();
			}, scheduler::main, 100ms);
		}

		void queue_cache_update(demonware::achievement_engine::cache_update update)
		{
			// dispatch() may run on the transport thread. Wait until the native claim
			// reply has unwound before borrowing its response bridge or issuing a task.
			scheduler::once([update = std::move(update)]
			{
				if (game::environment::is_dedicated() || game::environment::is_zombies()) return;
				try
				{
					if (!update.push_counters)
					{
						// Rollover: 13C480 compares unsigned progress at record+0x28 and only
						// writes larger values. One real user fetch replaces both counters.
						if (update.fetch_user) refresh_user_cache();
						return;
					}
					// The push below is a best-effort head start whose "in_progress" status
					// the 139C10 mapper may drop silently, so the fetch runs either way.
					if (update.fetch_user) schedule_user_cache_fetch();
					auto* context = game::AE_GetUserContext(0);
					if (!context || utils::hook::invoke<int>(0x7897A0_g, context) != 0) return;
					auto* bridge = game::AE_UserAchievementTaskData.get() + 0xF8;
					for (const auto& counter : update.counters)
					{
						const auto json = demonware::achievement_engine::counter_push(counter);
						if (json.empty() || !game::AE_SetResponseString(bridge, json.c_str())) return;
						demonware::hq_protocol::trace("counter_native_push", json);
						utils::hook::invoke<void>(0x13C480_g, context, bridge);
						console::info("[HQ AE] delivered counter push: %s %u/%u (%s)\n",
							counter.name.c_str(), counter.progress, counter.target,
							counter.progress >= counter.target ? "completed" : "inProgress");
					}
				}
				catch (const std::exception& error)
				{
					console::error("[HQ AE] counter cache update failed: %s\n", error.what());
				}
			}, scheduler::main, 100ms);
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
			demonware::achievement_engine::set_cache_update_sink(queue_cache_update);
			submit_hook.create(0x8397E0_g, submit_stub);
			scheduled_success_hook.create(game::AE_ScheduledTaskSucceeded, scheduled_success_stub);
			scheduled_failure_hook.create(game::AE_ScheduledTaskFailed, scheduled_failure_stub);
			command::add("aefetch", fetch_command);
			command::add("aecache", print_scheduled_cache);
		}

	};
}

REGISTER_COMPONENT(achievement_injection::component)
