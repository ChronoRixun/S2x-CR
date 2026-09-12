#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "component/console/console.hpp"
#include "game/game.hpp"

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
		// at least one message so the array is allocated, but a reply that fails, arrives
		// late or is emptied by a future change must not be able to crash the frontend
		// again, so refuse to run the poll while any ready block still has a null array.
		// See build/research/ghidra/decomp-crash/372069.c.
		constexpr std::ptrdiff_t mail_state_stride = 0x110;
		constexpr std::ptrdiff_t mail_state_ready = 0x24;
		constexpr std::ptrdiff_t mail_state_messages = 0xB0;
		constexpr std::size_t mail_state_blocks = 2;
		static_assert(mail_state_messages + sizeof(void*) <= mail_state_stride);

		utils::hook::detour poll_hook;

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
			return poll_hook.invoke<bool>();
		}
	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedicated() || game::environment::is_zombies()) return;
			poll_hook.create(game::MarketingComms_HasUnreadMail, poll_stub);
		}
	};
}

REGISTER_COMPONENT(mail_guard::component)
