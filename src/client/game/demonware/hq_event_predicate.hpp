#pragma once

#include "reward_game_event.hpp"
#include <charconv>
#include <string_view>

namespace demonware::hq_event_predicate
{
	struct rule
	{
		unsigned event_id{};
		std::string expression;
	};

	inline unsigned event_id(const std::string_view name)
	{
		unsigned id{};
		const auto parsed = std::from_chars(name.data(), name.data() + name.size(), id);
		if (!name.empty() && parsed.ec == std::errc{} && parsed.ptr == name.data() + name.size()) return id;
		// dw/dwgameevents.csv: transport accepts either numeric IDs or these names.
		if (name == "killed_a_player") return 1;
		if (name == "multi_kill") return 2;
		if (name == "gamemode_action") return 3;
		if (name == "streak") return 4;
		if (name == "end_game") return 5;
		if (name == "flak_gun_event") return 6;
		if (name == "one_v_one") return 7;
		if (name == "firing_range") return 8;
		if (name == "shootout") return 9;
		if (name == "social") return 10;
		if (name == "vendor") return 11;
		if (name == "supply_drop") return 12;
		if (name == "enter_hub") return 13;
		if (name == "player_rank_up") return 14;
		if (name == "division_rank_up") return 15;
		if (name == "zombies") return 16;
		if (name == "redeemed_challenge") return 17;
		if (name == "picked_up_payroll") return 18;
		if (name == "equippedSomethingInCAC") return 20;
		if (name == "completed_hq_onboard_phase1") return 21;
		if (name == "completed_hq_onboard_phase2") return 22;
		if (name == "completed_hq_onboard_phase3") return 23;
		if (name == "enter_scorestreak_training") return 24;
		if (name == "picked_up_orders") return 25;
		if (name == "completed_hub_fte") return 26;
		if (name == "social_score") return 27;
		if (name == "special_unlock") return 28;
		if (name == "_game_pump_mp") return 29;
		if (name == "_game_pump_zombies") return 30;
		if (name == "ranked_play_advance") return 31;
		if (name == "assists") return 32;
		if (name == "ugc_vote") return 33;
		if (name == "zombies_kills") return 34;
		if (name == "zombies_multikill") return 35;
		if (name == "zombies_jolts") return 36;
		if (name == "zombies_waves") return 37;
		if (name == "zombies_special") return 38;
		if (name == "zombies_st_patrick") return 39;
		if (name == "killed_a_zombie") return 40;
		if (name == "zombies_map_won") return 41;
		if (name == "zombies_dlc3_sv_unlock") return 42;
		if (name == "zombies_dlc3_ee_unlock") return 43;
		if (name == "zombies_dlc3_skull_unlock") return 44;
		if (name == "zombies_skin_unlock") return 45;
		if (name == "store") return 46;
		return 0;
	}

	struct result
	{
		bool valid{}, matches{};
	};

	// Grammar: or := and ("||" and)*; and := atom ("&&" atom)*;
	// atom := '(' or ')' | unsigned ':' unsigned. No implicit missing/zero values.
	// dwGameChallenges uses 128..130 for flag words (e.g. 130:4 && 130:128).
	class parser
	{
	public:
		parser(const std::string_view expression, const reward_game_events::event& event)
			: input_(expression), event_(event) {}

		result evaluate()
		{
			if (input_.size() > 2048 || event_.parameters.size() > 256) return {};
			// Reject ambiguous event parameters even on the direct task-12 path.
			bool seen[256]{};
			for (const auto& parameter : event_.parameters)
			{
				unsigned selector{};
				const auto& text = parameter.selector;
				const auto number = std::from_chars(text.data(), text.data() + text.size(), selector);
				if (text.empty() || number.ec != std::errc{} || number.ptr != text.data() + text.size() ||
					selector > 255 || seen[selector]) return {};
				seen[selector] = true;
			}
			if (input_.empty()) return {true, true};
			const auto matches = expression(0);
			return {valid_ && input_.empty(), valid_ && input_.empty() && matches};
		}

	private:
		bool take(const std::string_view token)
		{
			if (!input_.starts_with(token)) return false;
			input_.remove_prefix(token.size()); return true;
		}

		std::uint64_t number()
		{
			std::uint64_t value{};
			const auto parsed = std::from_chars(input_.data(), input_.data() + input_.size(), value);
			if (parsed.ec != std::errc{}) { valid_ = false; return 0; }
			input_.remove_prefix(static_cast<std::size_t>(parsed.ptr - input_.data()));
			return value;
		}

		bool atom(const unsigned depth)
		{
			if (depth >= 32) { valid_ = false; return false; }
			if (take("("))
			{
				const auto value = expression(depth + 1);
				if (!take(")")) valid_ = false;
				return value;
			}
			const auto selector = number();
			if (!take(":")) { valid_ = false; return false; }
			const auto expected = number();
			if (!valid_ || selector > 255) { valid_ = false; return false; }
			for (const auto& parameter : event_.parameters)
			{
				unsigned id{};
				std::from_chars(parameter.selector.data(), parameter.selector.data() + parameter.selector.size(), id);
				if (id == selector)
					return selector >= 128 ? (parameter.value & expected) == expected : parameter.value == expected;
			}
			return false;
		}

		bool conjunction(const unsigned depth)
		{
			auto value = atom(depth);
			while (valid_ && take("&&")) { const auto right = atom(depth); value = value && right; }
			return value;
		}

		bool expression(const unsigned depth)
		{
			auto value = conjunction(depth);
			// Always parse both sides, even if the left side decides the result.
			while (valid_ && take("||")) { const auto right = conjunction(depth); value = value || right; }
			return value;
		}

		std::string_view input_;
		const reward_game_events::event& event_;
		bool valid_ = true;
	};

	inline result evaluate(const std::string_view expression, const reward_game_events::event& event)
	{
		return parser{expression, event}.evaluate();
	}
}
