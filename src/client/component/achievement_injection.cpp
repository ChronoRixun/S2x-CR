#include <std_include.hpp>
#include "loader/component_loader.hpp"
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
		// See build/research/ae-internals.md. Only controller 0 is supported by
		// the existing response bridge; +0xF8 is also the native task stride.
		constexpr std::ptrdiff_t active_offset = 0xC0;
		constexpr std::ptrdiff_t transaction_offset = 0xD0;
		constexpr std::size_t transaction_size = 25;
		constexpr std::size_t request_capacity = 0x1800;
		game::symbol<bool(unsigned int, const void*)> fetch_scheduled{0x1399C0};
		game::symbol<std::byte> scheduled_task{0x60391D0};
		game::symbol<const char*(void*)> get_response_string{0xA3B850};
		utils::hook::detour fetch_user_hook;
		utils::hook::detour fetch_scheduled_hook;
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
			return index == 0 ? game::AE_UserAchievementTaskData.get() : scheduled_task.get();
		}

		std::string task_transaction(const std::byte* task)
		{
			const auto* value = reinterpret_cast<const char*>(task + transaction_offset);
			const auto* end = std::find(value, value + transaction_size, '\0');
			return end == value + transaction_size ? std::string{} : std::string{value, end};
		}

		void queue_response(const std::size_t index)
		{
			if (!accepting.load()) return;
			try
			{
				auto* task = task_data(index);
				const auto* request = get_response_string(task);
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

		bool fetch_user_stub(const unsigned int controller, const char* page,
			const void* transaction, const unsigned int account)
		{
			const auto result = fetch_user_hook.invoke<bool>(controller, page, transaction, account);
			if (result && controller == 0) queue_response(0);
			return result;
		}

		bool fetch_scheduled_stub(const unsigned int controller, const void* transaction)
		{
			const auto result = fetch_scheduled_hook.invoke<bool>(controller, transaction);
			if (result && controller == 0) queue_response(1);
			return result;
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
					// ProcessResponse routes by Action/ClientTx in group 0. Reuse the
					// proven Zombies string input synchronously on the main pipeline;
					// do not invent a scheduled-task response field.
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
	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedicated() || game::environment::is_zombies()) return;
			fetch_user_hook.create(game::AE_FetchUserAchievementsByPage, fetch_user_stub);
			fetch_scheduled_hook.create(fetch_scheduled, fetch_scheduled_stub);
			accepting = true;
			scheduler::loop(dispatch_responses, scheduler::pipeline::main, 50ms);
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
