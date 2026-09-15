#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace demonware
{
	class byte_buffer;

	namespace reward_game_events
	{
		struct parameter
		{
			std::string selector{};
			std::uint64_t value{};
		};

		struct event
		{
			std::string name{};
			std::int64_t timestamp{};
			std::vector<parameter> parameters{};
		};

		struct user_event_batch
		{
			std::uint64_t user_id{};
			std::string account_type{};
			std::vector<event> events{};
		};

		bool parse_report_request(byte_buffer* buffer, std::vector<event>& events, bool extended_parameters = false);
		bool parse_report_for_users_request(byte_buffer* buffer,
			std::vector<user_event_batch>& users, bool extended_parameters = false);

		// Same parsers; on a request-level rejection `reason` names the structural
		// check that failed (offsets, lengths, wire types - never payload bytes).
		bool parse_report_request(byte_buffer* buffer, std::vector<event>& events, bool extended_parameters,
			std::string& reason);
		bool parse_report_for_users_request(byte_buffer* buffer,
			std::vector<user_event_batch>& users, bool extended_parameters, std::string& reason);
	}

	enum class reward_delivery { applied, queued, retryable_failure };

	bool submit_hq_event(const reward_game_events::event& event);

	// bdReward.cpp: the single task-11 routing path (local store or relay, then
	// hidden challenges). Shared with the `hqrelaytest` synthetic batch command.
	reward_delivery route_reward_user_event(std::uint64_t user_id, reward_game_events::event& event);
}
