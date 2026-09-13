#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "component/console/console.hpp"
#include "game/game.hpp"
#include "game/demonware/hq_mail.hpp"

#include <utils/hook.hpp>

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
			console::info("[HQ mail] %s controller=%d category=%d index=%d mapped=%d count=%u inRange=%u; local inbox has no claimable messages\n",
				action, controller, category, index, slot, count, unsigned(valid));
		}

		bool message_stub(int controller, int category, int index, char* output, int capacity)
		{
			++demonware::hq_mail::native_reads;
			trace_access("read", controller, category, index);
			if (output && capacity > 0) *output = 0;
			// Explicit MP empty-inbox policy. 125020 returns zero Lua values on false.
			// Do not let 3722F0 dereference an unchecked category-to-slot result.
			return false;
		}

		void redeem_stub(int controller, int category, int index)
		{
			++demonware::hq_mail::native_redeems;
			trace_access("redeem suppressed", controller, category, index);
			// No fabricated code/reward and no native task from a cleared/stale UI slot.
		}

		void success_stub(void* task)
		{
			success_hook.invoke<void>(task);
			const auto controller = *reinterpret_cast<const int*>(static_cast<const std::byte*>(task) + 4);
			if (controller < 0 || controller >= 2) return;
			const auto* state = game::MarketingComms_MailState.get() + controller * mail_state_stride;
			console::info("[HQ mail] native fetch success controller=%d ready=%d count=%u capacity=%u slots=%p; empty-inbox policy active\n",
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
			// The local inbox advertises no unread messages; keep the allocation intact.
			return false;
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
		}
	};
}

REGISTER_COMPONENT(mail_guard::component)
