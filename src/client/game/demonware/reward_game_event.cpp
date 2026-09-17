#include <std_include.hpp>

#include "reward_game_event.hpp"
#include "byte_buffer.hpp"
#include "hq_protocol.hpp"
#include "component/console/console.hpp"
#include <charconv>
#include <string>

namespace demonware::reward_game_events
{
	namespace
	{
		constexpr auto maximum_reported_events = 100u;
		constexpr auto maximum_reported_users = 48u;
		constexpr auto maximum_event_parameters = 10u;
		constexpr auto maximum_report_request_size = 64u * 1024u;
		// Task 11 can contain 48 user batches with up to 100 events each.
		constexpr auto maximum_report_for_users_request_size = 3u * 1024u * 1024u;
		// One full AES block: the cipher pads 1..16 bytes, so a payload whose length is
		// already a multiple of 16 arrives with sixteen zero bytes after it (the live
		// end-of-match task 11 batch does exactly that).
		constexpr auto maximum_encryption_padding = 16u;

		class struct_buffer_reader
		{
		public:
			explicit struct_buffer_reader(const std::string_view data)
				: data_(data)
			{
			}

			bool empty() const
			{
				return data_.empty();
			}

			std::size_t remaining() const
			{
				return data_.size();
			}

			bool read_tag(std::uint32_t& field, std::uint8_t& wire_type)
			{
				std::uint64_t tag{};
				if (!read_varint(tag) || tag > std::numeric_limits<std::uint32_t>::max())
				{
					return false;
				}

				field = static_cast<std::uint32_t>(tag >> 3);
				wire_type = static_cast<std::uint8_t>(tag & 7);
				return field != 0;
			}

			bool read_varint(std::uint64_t& value)
			{
				value = 0;
				for (auto index = 0u; index < 10; ++index)
				{
					std::uint8_t byte{};
					if (!read_byte(byte) || (index == 9 && (byte & 0xFE) != 0))
					{
						return false;
					}

					value |= static_cast<std::uint64_t>(byte & 0x7F) << (index * 7);
					if ((byte & 0x80) == 0)
					{
						return true;
					}
				}

				return false;
			}

			bool read_length_delimited(std::string_view& value)
			{
				std::uint64_t length{};
				if (!read_varint(length) || length > data_.size())
				{
					return false;
				}

				value = data_.substr(0, static_cast<std::size_t>(length));
				data_.remove_prefix(static_cast<std::size_t>(length));
				return true;
			}

			bool skip_field(const std::uint8_t wire_type)
			{
				switch (wire_type)
				{
				case 0:
				{
					std::uint64_t value{};
					return read_varint(value);
				}
				case 1:
					return skip_bytes(8);
				case 2:
				{
					std::string_view value{};
					return read_length_delimited(value);
				}
				case 5:
					return skip_bytes(4);
				default:
					return false;
				}
			}

		private:
			bool read_byte(std::uint8_t& value)
			{
				if (data_.empty())
				{
					return false;
				}

				value = static_cast<std::uint8_t>(data_.front());
				data_.remove_prefix(1);
				return true;
			}

			bool skip_bytes(const std::size_t count)
			{
				if (count > data_.size())
				{
					return false;
				}

				data_.remove_prefix(count);
				return true;
			}

			std::string_view data_{};
		};

		// A single event of a batch can be invalid on its own - a duplicate selector, a
		// selector above 255, truncated parameters. Its bytes are length delimited, so
		// the reader stays in sync and the event can be dropped on its own; rejecting the
		// whole request instead threw away every other event and every other user.
		struct skip_reason
		{
			const char* key{};    // rate limit bucket: short, and one of a fixed set
			std::string detail{}; // what the console line reports
		};

		struct batch_context
		{
			const char* request{};
			int user_slot{-1};    // negative when the request carries no user batches
		};

		bool skip_due(const batch_context& batch, const skip_reason& reason)
		{
			return hq_protocol::report_due(std::string{"reward/"} + batch.request + "/" + reason.key);
		}

		void report_skipped_event(const batch_context& batch, const std::size_t index,
			const skip_reason& reason)
		{
			if (!skip_due(batch, reason))
			{
				return;
			}

			if (batch.user_slot < 0)
			{
				console::warn("[hidden_challenges] %s: skipped event %u (%s)\n",
					batch.request, static_cast<unsigned>(index), reason.detail.c_str());
				return;
			}

			console::warn("[hidden_challenges] %s: skipped user slot %d event %u (%s)\n",
				batch.request, batch.user_slot, static_cast<unsigned>(index), reason.detail.c_str());
		}

		void report_skipped_user(const batch_context& batch, const skip_reason& reason)
		{
			if (!skip_due(batch, reason))
			{
				return;
			}

			console::warn("[hidden_challenges] %s: skipped user slot %d (%s)\n",
				batch.request, batch.user_slot, reason.detail.c_str());
		}

		bool is_valid_string(const std::string_view value, const std::size_t maximum_length)
		{
			return !value.empty() && value.size() <= maximum_length &&
				value.find('\0') == std::string_view::npos;
		}

		bool parse_parameter(const std::string_view data, parameter& result)
		{
			// RewardGameEventValue: 1 = selector, 2 = uint64 value.
			struct_buffer_reader reader{data};
			bool has_selector{};
			bool has_value{};
			while (!reader.empty())
			{
				std::uint32_t field{};
				std::uint8_t wire_type{};
				if (!reader.read_tag(field, wire_type))
				{
					return false;
				}

				if (field == 1)
				{
					std::string_view selector{};
					if (wire_type != 2 || has_selector ||
						!reader.read_length_delimited(selector) || !is_valid_string(selector, 19))
					{
						return false;
					}

					result.selector.assign(selector);
					has_selector = true;
				}
				else if (field == 2)
				{
					if (wire_type != 0 || has_value || !reader.read_varint(result.value))
					{
						return false;
					}

					has_value = true;
				}
				else if (!reader.skip_field(wire_type))
				{
					return false;
				}
			}

			return has_selector && has_value;
		}

		// MP events address their values by a numeric selector; the same selector twice,
		// or one above the byte the game stores it in, is what the live batches carry.
		bool normalise_selector(parameter& value, const std::vector<parameter>& existing,
			skip_reason& reason)
		{
			std::uint64_t selector{};
			const auto* const first = value.selector.data();
			const auto* const last = first + value.selector.size();
			const auto parsed = std::from_chars(first, last, selector);
			if (parsed.ec != std::errc{} || parsed.ptr != last)
			{
				reason = {"selector_format", "non-numeric selector '" + value.selector + "'"};
				return false;
			}

			if (selector > 255)
			{
				reason = {"selector_range", "selector " + std::to_string(selector) + " > 255"};
				return false;
			}

			value.selector = std::to_string(selector);
			for (const auto& prior : existing)
			{
				if (prior.selector == value.selector)
				{
					reason = {"duplicate_selector", "duplicate selector " + value.selector};
					return false;
				}
			}

			return true;
		}

		bool parse_event(const std::string_view data, event& result, const bool extended_parameters,
			skip_reason& reason)
		{
			// RewardGameEvent: 1 = name, 3 = ZigZag timestamp, 4 = repeated values.
			struct_buffer_reader reader{data};
			bool has_name{};
			bool has_timestamp{};
			while (!reader.empty())
			{
				std::uint32_t field{};
				std::uint8_t wire_type{};
				if (!reader.read_tag(field, wire_type))
				{
					reason = {"malformed_event", "malformed field header"};
					return false;
				}

				if (field == 1)
				{
					std::string_view name{};
					if (wire_type != 2 || has_name || !reader.read_length_delimited(name) ||
						!is_valid_string(name, 99))
					{
						reason = {"invalid_name", "missing or invalid event name"};
						return false;
					}

					result.name.assign(name);
					has_name = true;
				}
				else if (field == 3)
				{
					std::uint64_t encoded{};
					if (wire_type != 0 || has_timestamp || !reader.read_varint(encoded))
					{
						reason = {"invalid_timestamp", "missing or invalid timestamp"};
						return false;
					}

					const auto magnitude = static_cast<std::int64_t>(encoded >> 1);
					result.timestamp = (encoded & 1) != 0 ? -magnitude - 1 : magnitude;
					has_timestamp = true;
				}
				else if (field == 4)
				{
					const auto parameter_limit = extended_parameters ? 256u : maximum_event_parameters;
					std::string_view parameter_data{};
					if (wire_type != 2 || !reader.read_length_delimited(parameter_data))
					{
						reason = {"truncated_parameters", "truncated parameters"};
						return false;
					}

					if (result.parameters.size() >= parameter_limit)
					{
						reason = {"parameter_limit",
							"more than " + std::to_string(parameter_limit) + " parameters"};
						return false;
					}

					parameter value{};
					if (!parse_parameter(parameter_data, value))
					{
						reason = {"malformed_parameter",
							"malformed parameter " + std::to_string(result.parameters.size())};
						return false;
					}

					if (extended_parameters && !normalise_selector(value, result.parameters, reason))
					{
						return false;
					}

					result.parameters.push_back(std::move(value));
				}
				else if (!reader.skip_field(wire_type))
				{
					reason = {"malformed_event", "unreadable field " + std::to_string(field)};
					return false;
				}
			}

			if (!has_name || !has_timestamp)
			{
				reason = {"incomplete_event", "missing event name or timestamp"};
				return false;
			}

			return true;
		}

		bool parse_user_account(const std::string_view data, user_event_batch& result)
		{
			// UserAccountID: 1 = user ID, 2 = account type.
			struct_buffer_reader reader{data};
			bool has_user_id{};
			bool has_account_type{};
			while (!reader.empty())
			{
				std::uint32_t field{};
				std::uint8_t wire_type{};
				if (!reader.read_tag(field, wire_type))
				{
					return false;
				}

				if (field == 1)
				{
					if (wire_type != 0 || has_user_id || !reader.read_varint(result.user_id))
					{
						return false;
					}

					has_user_id = true;
				}
				else if (field == 2)
				{
					std::string_view account_type{};
					if (wire_type != 2 || has_account_type ||
						!reader.read_length_delimited(account_type) ||
						!is_valid_string(account_type, 9))
					{
						return false;
					}

					result.account_type.assign(account_type);
					has_account_type = true;
				}
				else if (!reader.skip_field(wire_type))
				{
					return false;
				}
			}

			return has_user_id && has_account_type;
		}

		bool parse_user_event_batch(const std::string_view data, user_event_batch& result,
			const bool extended_parameters, const batch_context& batch)
		{
			// UserEventBatch: 1 = account, 2 = repeated events, 3 = transaction ID.
			struct_buffer_reader reader{data};
			bool has_account{};
			bool has_transaction_id{};
			std::size_t event_index{};
			while (!reader.empty())
			{
				std::uint32_t field{};
				std::uint8_t wire_type{};
				if (!reader.read_tag(field, wire_type))
				{
					return false;
				}

				if (field == 1)
				{
					std::string_view account_data{};
					if (wire_type != 2 || has_account ||
						!reader.read_length_delimited(account_data) ||
						!parse_user_account(account_data, result))
					{
						return false;
					}

					has_account = true;
				}
				else if (field == 2)
				{
					std::string_view event_data{};
					if (wire_type != 2 || !reader.read_length_delimited(event_data))
					{
						// The framing itself is broken, so the reader cannot
						// resynchronise: the rest of this batch is unreadable.
						return false;
					}

					const auto index = event_index++;
					if (result.events.size() >= maximum_reported_events)
					{
						report_skipped_event(batch, index, {"event_limit",
							"more than " + std::to_string(maximum_reported_events) + " events"});
						continue;
					}

					event value{};
					skip_reason reason{};
					if (!parse_event(event_data, value, extended_parameters, reason))
					{
						report_skipped_event(batch, index, reason);
						continue;
					}

					result.events.push_back(std::move(value));
				}
				else if (field == 3)
				{
					std::string_view transaction_id{};
					if (wire_type != 2 || has_transaction_id ||
						!reader.read_length_delimited(transaction_id) ||
						!is_valid_string(transaction_id, 24))
					{
						return false;
					}

					has_transaction_id = true;
				}
				else if (!reader.skip_field(wire_type))
				{
					return false;
				}
			}

			return has_account;
		}

		// A request-level rejection drops every event of every user, so the warning
		// names the structural check that failed: field numbers, wire types, offsets
		// and lengths only - never the payload bytes.
		std::string describe_offset(const std::string_view data, const struct_buffer_reader& reader)
		{
			return "offset " + std::to_string(data.size() - reader.remaining()) + " of " +
				std::to_string(data.size());
		}

		bool parse_report_payload(const std::string_view data, std::vector<event>& events,
			const bool extended_parameters, const batch_context& batch, std::string& reason)
		{
			// ReportRewardGameEventsRequest: 1 = context, 2 = repeated events,
			// 3 = transaction ID.
			struct_buffer_reader reader{data};
			bool has_context{};
			bool has_transaction_id{};
			std::size_t event_index{};
			while (!reader.empty())
			{
				std::uint32_t field{};
				std::uint8_t wire_type{};
				if (!reader.read_tag(field, wire_type))
				{
					reason = "unreadable field tag at " + describe_offset(data, reader);
					return false;
				}

				if (field == 1)
				{
					if (wire_type != 2 || has_context)
					{
						reason = has_context ? std::string{"second context field"} :
							"context field with wire type " + std::to_string(wire_type);
						return false;
					}

					std::string_view context{};
					if (!reader.read_length_delimited(context))
					{
						reason = "truncated context field at " + describe_offset(data, reader);
						return false;
					}

					if (!is_valid_string(context, 15))
					{
						reason = "context string of " + std::to_string(context.size()) +
							" bytes is invalid (limit 15, no NUL)";
						return false;
					}

					has_context = true;
				}
				else if (field == 2)
				{
					std::string_view event_data{};
					if (wire_type != 2 || !reader.read_length_delimited(event_data))
					{
						reason = "event " + std::to_string(event_index) + " with wire type " +
							std::to_string(wire_type) + " at " + describe_offset(data, reader);
						return false;
					}

					const auto index = event_index++;
					if (events.size() >= maximum_reported_events)
					{
						report_skipped_event(batch, index, {"event_limit",
							"more than " + std::to_string(maximum_reported_events) + " events"});
						continue;
					}

					event value{};
					skip_reason skip{};
					if (!parse_event(event_data, value, extended_parameters, skip))
					{
						report_skipped_event(batch, index, skip);
						continue;
					}

					events.push_back(std::move(value));
				}
				else if (field == 3)
				{
					if (wire_type != 2 || has_transaction_id)
					{
						reason = has_transaction_id ? std::string{"second transaction id field"} :
							"transaction id field with wire type " + std::to_string(wire_type);
						return false;
					}

					std::string_view transaction_id{};
					if (!reader.read_length_delimited(transaction_id))
					{
						reason = "truncated transaction id field at " + describe_offset(data, reader);
						return false;
					}

					if (!is_valid_string(transaction_id, 24))
					{
						reason = "transaction id of " + std::to_string(transaction_id.size()) +
							" bytes is invalid (limit 24, no NUL)";
						return false;
					}

					has_transaction_id = true;
				}
				else if (!reader.skip_field(wire_type))
				{
					reason = "field " + std::to_string(field) + " with wire type " +
						std::to_string(wire_type) + " could not be skipped at " + describe_offset(data, reader);
					return false;
				}
			}

			if (!has_context)
			{
				reason = "no context field (" + std::to_string(events.size()) + " events parsed, " +
					std::to_string(data.size()) + " payload bytes)";
				return false;
			}

			return true;
		}

		bool parse_report_for_users_payload(const std::string_view data,
			std::vector<user_event_batch>& users, const bool extended_parameters,
			const char* const request, std::string& reason)
		{
			// ReportRewardGameEventsForUsersRequest: 1 = context, 2 = repeated users.
			struct_buffer_reader reader{data};
			bool has_context{};
			std::size_t user_index{};
			while (!reader.empty())
			{
				std::uint32_t field{};
				std::uint8_t wire_type{};
				if (!reader.read_tag(field, wire_type))
				{
					reason = "unreadable field tag at " + describe_offset(data, reader);
					return false;
				}

				if (field == 1)
				{
					if (wire_type != 2 || has_context)
					{
						reason = has_context ? std::string{"second context field"} :
							"context field with wire type " + std::to_string(wire_type);
						return false;
					}

					std::string_view context{};
					if (!reader.read_length_delimited(context))
					{
						reason = "truncated context field at " + describe_offset(data, reader);
						return false;
					}

					if (!is_valid_string(context, 15))
					{
						reason = "context string of " + std::to_string(context.size()) +
							" bytes is invalid (limit 15, no NUL)";
						return false;
					}

					has_context = true;
				}
				else if (field == 2)
				{
					std::string_view user_data{};
					if (wire_type != 2 || !reader.read_length_delimited(user_data))
					{
						reason = "user batch " + std::to_string(user_index) + " with wire type " +
							std::to_string(wire_type) + " at " + describe_offset(data, reader);
						return false;
					}

					const batch_context batch{request, static_cast<int>(user_index++)};
					if (users.size() >= maximum_reported_users)
					{
						report_skipped_user(batch, {"user_limit",
							"more than " + std::to_string(maximum_reported_users) + " user batches"});
						continue;
					}

					user_event_batch user{};
					if (!parse_user_event_batch(user_data, user, extended_parameters, batch))
					{
						report_skipped_user(batch, {"malformed_user", "malformed user batch"});
						continue;
					}

					users.push_back(std::move(user));
				}
				else if (!reader.skip_field(wire_type))
				{
					reason = "field " + std::to_string(field) + " with wire type " +
						std::to_string(wire_type) + " could not be skipped at " + describe_offset(data, reader);
					return false;
				}
			}

			if (!has_context)
			{
				reason = "no context field (" + std::to_string(users.size()) + " user batches parsed, " +
					std::to_string(data.size()) + " payload bytes)";
				return false;
			}

			return true;
		}

		bool read_payload(byte_buffer* buffer, std::string& payload, const std::size_t maximum_size,
			std::string& reason)
		{
			// Lobby tasks wrap StructBuffer in a typed bdByteBuffer structured-data value.
			// Decryption can leave up to one AES block of zero padding after that value.
			if (!buffer)
			{
				reason = "no request buffer";
				return false;
			}

			const auto available = buffer->get_remaining().size();
			if (!buffer->read_struct(&payload, maximum_size))
			{
				reason = "typed payload could not be read from " + std::to_string(available) +
					" bytes (limit " + std::to_string(maximum_size) + ")";
				return false;
			}

			const auto padding = buffer->get_remaining();
			const auto non_zero = static_cast<std::size_t>(std::ranges::count_if(padding, [](const char value)
			{
				return value != '\0';
			}));
			if (padding.size() > maximum_encryption_padding || non_zero != 0)
			{
				reason = std::to_string(padding.size()) + " trailing bytes after the " +
					std::to_string(payload.size()) + "-byte payload (" + std::to_string(non_zero) +
					" non-zero; at most " + std::to_string(maximum_encryption_padding) + " zero bytes allowed)";
				return false;
			}

			return true;
		}
	}

	bool parse_report_request(byte_buffer* buffer, std::vector<event>& events, const bool extended_parameters,
		std::string& reason)
	{
		std::string payload{};
		std::vector<event> parsed{};
		if (!read_payload(buffer, payload, maximum_report_request_size, reason) ||
			!parse_report_payload(payload, parsed, extended_parameters, {"task 12", -1}, reason))
		{
			return false;
		}

		events = std::move(parsed);
		return true;
	}

	bool parse_report_request(byte_buffer* buffer, std::vector<event>& events, const bool extended_parameters)
	{
		std::string reason{};
		return parse_report_request(buffer, events, extended_parameters, reason);
	}

	bool parse_report_for_users_request(byte_buffer* buffer,
		std::vector<user_event_batch>& users, const bool extended_parameters, std::string& reason)
	{
		std::string payload{};
		std::vector<user_event_batch> parsed{};
		if (!read_payload(buffer, payload, maximum_report_for_users_request_size, reason) ||
			!parse_report_for_users_payload(payload, parsed, extended_parameters, "task 11", reason))
		{
			return false;
		}

		users = std::move(parsed);
		return true;
	}

	bool parse_report_for_users_request(byte_buffer* buffer,
		std::vector<user_event_batch>& users, const bool extended_parameters)
	{
		std::string reason{};
		return parse_report_for_users_request(buffer, users, extended_parameters, reason);
	}
}
