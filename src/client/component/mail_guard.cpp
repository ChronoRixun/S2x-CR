#include <std_include.hpp>
#include "game/demonware/hq_logging.hpp"
#include "loader/component_loader.hpp"
#include "component/console/console.hpp"
#include "game/game.hpp"
#include "game/demonware/hq_mail.hpp"

#include <utils/hook.hpp>
#include "component/scheduler.hpp"
#include "game/ui_scripting/execution.hpp"
#include "component/ui_scripting.hpp"

namespace mail_guard
{
	namespace
	{
		// Belt and braces for the Marketing Comms / Mail crash (0xC0000005 at image offset
		// 0x372069, ~2 s after the Headquarters background loads). The poll at 0x372020,
		// called every frame from 0x852630, walks two 0x110-byte per-controller mail state
		// blocks starting at 0x8A14F60. For each block whose ready flag (+0x24) is set it
		// reads message entries out of the array at +0xB0 - without ever checking that the
		// array pointer is non-null. bdMarketingComms::getMessages now always answers with
		// at least 14 messages so the array is allocated, but a reply that fails, arrives
		// late or is emptied by a future change must not be able to crash the frontend
		// again, so refuse to run the poll while any ready block still has a null array.
		// See build/research/ghidra/decomp-crash/372069.c.
		constexpr std::ptrdiff_t mail_state_stride = 0x110;
		constexpr std::ptrdiff_t mail_state_ready = 0x24;
		constexpr std::ptrdiff_t mail_state_messages = 0xB0;
		constexpr std::size_t mail_state_blocks = 2;
		static_assert(mail_state_messages + sizeof(void*) <= mail_state_stride);

		utils::hook::detour poll_hook;
		utils::hook::detour message_hook;
		utils::hook::detour redeem_hook;
		utils::hook::detour success_hook;

		void trace_access(const char* action, const int controller, const int category, const int index)
		{
			if (controller < 0 || controller >= 2)
			{
				++demonware::hq_mail::rejected_indices;
				console::warn("[HQ mail] %s rejected controller=%d\n", action, controller);
				return;
			}
			const auto* state = game::MarketingComms_MailState.get() + controller * mail_state_stride;
			const auto count = *reinterpret_cast<const unsigned*>(state + 0xBC);
			const auto slot = utils::hook::invoke<int>(0x3723B0_g, category, index);
			const auto valid = demonware::hq_mail::valid_slot(controller, slot, count);
			if (!valid) ++demonware::hq_mail::rejected_indices;
			// Capture initial polls and changes without flooding the per-frame console.
			static int last_controller = -1, last_category = -1, last_index = -1;
			static unsigned logged{};
			if (action[0] == 'r' && action[2] == 'a' && logged++ >= 32 &&
				last_controller == controller && last_category == category && last_index == index) return;
			last_controller = controller; last_category = category; last_index = index;
			console::info("[HQ mail] %s controller=%d category=%d index=%d mapped=%d count=%u inRange=%u; local delivery policy\n",
				action, controller, category, index, slot, count, unsigned(valid));
		}

		const std::byte* checked_slot(const int controller, const int slot)
		{
			if (controller != 0) return nullptr;
			const auto* state = game::MarketingComms_MailState.get() + controller * mail_state_stride;
			const auto count = *reinterpret_cast<const unsigned*>(state + 0xBC);
			const auto capacity = *reinterpret_cast<const unsigned*>(state + 0xB8);
			const auto* messages = *reinterpret_cast<const std::byte* const*>(state + 0xB0);
			if (!messages || !*reinterpret_cast<const unsigned*>(state + 0x24) || capacity < count ||
				!demonware::hq_mail::valid_slot(controller, slot, count)) return nullptr;
			const auto* result = messages + slot * 0x1CA0;
			if (*reinterpret_cast<const unsigned*>(result + 0x102C) > 4096 ||
				*reinterpret_cast<const unsigned*>(result + 0x1830) > 2048 ||
				*reinterpret_cast<const unsigned*>(result + 0x1C34) > 1024 ||
				*reinterpret_cast<const unsigned*>(result + 0x1C78) > 64) return nullptr;
			return result;
		}

		bool redeem_slot(const int controller, const int slot)
		{
			const auto* message = checked_slot(controller, slot);
			if (!message) return false;
			const auto id = *reinterpret_cast<const std::uint64_t*>(message + 0x10);
			const auto length = *reinterpret_cast<const unsigned*>(message + 0x1C34);
			const std::string code{reinterpret_cast<const char*>(message + 0x1834), length};
			const auto ok = demonware::hq_economy::transact([&](auto& state) { return demonware::hq_mail::redeem(state, id, code); });
			if (ok)
			{
				// Native clear operation 0x3721A0 also clears just ID. Keep all slots allocated.
				*const_cast<std::uint64_t*>(reinterpret_cast<const std::uint64_t*>(message + 0x10)) = 0;
			}
			console::info("[HQ mail] redeem id=%llu slot=%d success=%d\n", id, slot, ok);
			// Voucher kiosk opens its popup after this call returns. Deliver the recovered
			// ApplyConversionRule completion after that, using its existing refresh handler.
			scheduler::once([controller, ok]
			{
				static std::atomic_bool warned{};
				try
				{
					if (!game::environment::is_zombies()) ui_scripting::notify("inventory", {
						{"controller", controller}, {"inventoryEventType", 4}, {"inventoryTaskType", 126}, {"success", ok}});
				}
				catch (const std::exception& error)
				{
					demonware::hq_logging::safe_warn_once(warned, "[HQ callback] voucher inventory notification: %s\n", error.what());
				}
				catch (...)
				{
					demonware::hq_logging::safe_warn_once(warned, "[HQ callback] voucher inventory notification: unknown exception\n");
				}
			}, scheduler::pipeline::main, 250ms);
			// Report the grant, not merely that a valid slot was found: Engine.
			// Inventory_RedeemVoucherItem returns this synchronously to the kiosk Lua, so a
			// failed transact() must not read as a successful redemption. The message id is
			// deliberately left uncleared above, which keeps the delivery claimable.
			return ok;
		}

		bool message_stub(int controller, int category, int index, char* output, int capacity)
		{
			++demonware::hq_mail::native_reads;
			trace_access("read", controller, category, index);
			if (output && capacity > 0) *output = 0;
			if (controller != 0 || category < 1 || category > 5 || index < 0 || index >= 14 || !output || capacity <= 0) return false;
			const auto slot = utils::hook::invoke<int>(0x3723B0_g, category, index);
			if (!checked_slot(controller, slot)) return false;
			return message_hook.invoke<bool>(controller, category, index, output, capacity);
		}

		void redeem_stub(int controller, int category, int index)
		{
			++demonware::hq_mail::native_redeems;
			trace_access("redeem", controller, category, index);
			if (controller != 0 || category < 1 || category > 5 || index < 0 || index >= 14) return;
			(void)redeem_slot(controller, utils::hook::invoke<int>(0x3723B0_g, category, index));
		}

		void install_mail_ui()
		{
			if (game::environment::is_zombies()) return;
			const auto lua = ui_scripting::get_globals();
			ui_scripting::table api;
			api["List"] = [](int controller)
			{
				ui_scripting::table result;
				if (controller != 0 || game::environment::is_zombies()) return result;
				try
				{
					const auto state = demonware::hq_economy::snapshot();
					int index{};
					for (std::size_t i = 0; i < demonware::hq_mail::deliveries.size() && i < 6; ++i)
					{
						const auto& message = demonware::hq_mail::deliveries[i];
						const auto* slot = checked_slot(controller, static_cast<int>(8 + i));
						if (!slot || *reinterpret_cast<const std::uint64_t*>(slot + 0x10) != message.id || !demonware::hq_mail::pending(state, message)) continue;
						ui_scripting::table item;
						item["guid"] = utils::string::va("0x%x", 0x50E0001u + static_cast<unsigned>(i));
						item["slot"] = static_cast<int>(8 + i); item["name"] = message.title; item["desc"] = message.description;
						item["image"] = "s2_armory_credits_icon"; item["itemQuantity"] = 1;
						result[++index] = item;
					}
				}
				catch (const std::exception& e) { console::warn("[HQ mail] list: %s\n", e.what()); }
				return result;
			};
			api["Redeem"] = [](int controller, int slot) { return !game::environment::is_zombies() && redeem_slot(controller, slot); };
			lua["S2xHQMail"] = api;
			(void)lua["loadstring"](R"lua(
local voucherList = Engine.Inventory_GetVoucherItems
local lootData = InventoryUtils.GetLootData
local redeem = Engine.Inventory_RedeemVoucherItem
local pending = {}
Engine.Inventory_GetVoucherItems = function(controller, ...)
	if CONDITIONS.IsZombiesMode() then return voucherList(controller, ...) end
	local result = voucherList(controller, ...) or {}
	if not CONDITIONS.IsZombiesMode() then
		pending = {}
		for _, message in ipairs(S2xHQMail.List(controller)) do
			pending[message.guid] = message
			result[#result + 1] = {itemID = message.guid}
		end
	end
	return result
end
InventoryUtils.GetLootData = function(guid, ...)
	if not CONDITIONS.IsZombiesMode() and pending[guid] then return pending[guid] end
	return lootData(guid, ...)
end
Engine.Inventory_RedeemVoucherItem = function(controller, guid, ...)
	if not CONDITIONS.IsZombiesMode() and pending[guid] then
		return S2xHQMail.Redeem(controller, pending[guid].slot)
	end
	return redeem(controller, guid, ...)
end
)lua")[0]();
		}

		void success_stub(void* task)
		{
			success_hook.invoke<void>(task);
			const auto controller = *reinterpret_cast<const int*>(static_cast<const std::byte*>(task) + 4);
			if (controller < 0 || controller >= 2) return;
			const auto* state = game::MarketingComms_MailState.get() + controller * mail_state_stride;
			console::info("[HQ mail] native fetch success controller=%d ready=%d count=%u capacity=%u slots=%p; delivery policy active\n",
				controller, *reinterpret_cast<const int*>(state + 0x24),
				*reinterpret_cast<const unsigned*>(state + 0xBC), *reinterpret_cast<const unsigned*>(state + 0xB8),
				*reinterpret_cast<void* const*>(state + 0xB0));
		}

		bool has_null_message_array()
		{
			const auto* base = game::MarketingComms_MailState.get();
			for (std::size_t i = 0; i < mail_state_blocks; ++i)
			{
				const auto* block = base + i * mail_state_stride;
				if (*reinterpret_cast<const int*>(block + mail_state_ready) == 0) continue;
				if (*reinterpret_cast<void* const*>(block + mail_state_messages) == nullptr) return true;
			}
			return false;
		}

		bool poll_stub()
		{
			if (has_null_message_array())
			{
				static auto warned = false;
				if (!warned)
				{
					warned = true;
					console::warn("[HQ mail] unread-mail poll suppressed: message array is null "
						"while the mail state is flagged ready\n");
				}
				return false;
			}
			// The native breadcrumb polls every frame; do not copy the full economy at 60 Hz.
			static std::chrono::steady_clock::time_point checked{};
			static bool unread{};
			const auto now = std::chrono::steady_clock::now();
			if (now - checked < 1s) return unread;
			checked = now;
			try
			{
				const auto state = demonware::hq_economy::snapshot();
				unread = std::any_of(demonware::hq_mail::deliveries.begin(), demonware::hq_mail::deliveries.end(),
					[&](const auto& d) { return demonware::hq_mail::pending(state, d); });
			}
			catch (...) { unread = false; }
			return unread;
		}
	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedicated() || game::environment::is_zombies()) return;
			poll_hook.create(game::MarketingComms_HasUnreadMail, poll_stub);
			message_hook.create(0x3722F0_g, message_stub);
			redeem_hook.create(0x3726F0_g, redeem_stub);
			success_hook.create(0x3726A0_g, success_stub);
			ui_scripting::on_start(install_mail_ui);
		}
	};
}

REGISTER_COMPONENT(mail_guard::component)
