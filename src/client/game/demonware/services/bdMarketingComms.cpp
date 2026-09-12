#include <std_include.hpp>
#include "../dw_include.hpp"

#include "game/game.hpp"
#include "../hq_protocol.hpp"
#include "../hq_mail.hpp"

namespace demonware
{
	bdMarketingComms::bdMarketingComms() : service(104, "bdMarketingComms")
	{
		this->register_task(4, &bdMarketingComms::reportFullMessagesViewed);
		this->register_task(6, &bdMarketingComms::getMessages);
	}

	void bdMarketingComms::reportFullMessagesViewed(service_server* server, byte_buffer* buffer) const
	{
		hq_protocol::trace("marketing_4", buffer->get_remaining());
		auto reply = server->create_reply(this->task_id());
		reply.send();
	}

#pragma pack(push, 1)
	struct bdCommsGetMessagesRequest
	{
		char __pad0[23];
	}; static_assert(sizeof(bdCommsGetMessagesRequest) == 23);

	struct unk_s
	{
		char __pad0[17];
		unsigned char unk;
	};

	struct bdCommsGetMessagesResponse
	{
		unk_s unk[1];
	}; //static_assert(sizeof(bdCommsGetMessagesResponse) == 180);
#pragma pack(pop)

	namespace
	{
		// The getMessages request body is protobuf; a live capture is
		// build/research/crash-372069/attempt1-dw/hq_marketing_6_3532_22.bin:
		//   1: ""  2: "en-US"  3: repeated { 1: category, 2: count }
		//   -> {1,4} {2,4} {3,1} {4,4} {5,1} = 14 advertised message slots.
		//
		// The frontend's "is there unread mail?" poll (image offset 0x372020, run every
		// frame from 0x852630, decompile in build/research/ghidra/decomp-crash/372069.c)
		// walks the deserialised message array from the global start index at 0x8A15B88
		// (= 8) and never validates either the array pointer or the reply length. An empty
		// collection leaves that pointer NULL and the poll faults reading NULL+0xF52C about
		// two seconds after the Headquarters background loads; a single fabricated message
		// is still read out of bounds (the long-standing "Mail errors"). So the reply must
		// carry one benign message per advertised slot.
		constexpr std::size_t default_message_slots = 14;
		constexpr std::size_t max_request_categories = 64;
		constexpr std::uint64_t max_slots_per_category = 64;

		bool read_varint(const std::string& body, std::size_t& at, std::uint64_t& value)
		{
			value = 0;
			for (auto shift = 0; shift < 64; shift += 7)
			{
				if (at >= body.size()) return false;
				const auto byte = static_cast<unsigned char>(body[at++]);
				if (shift == 63 && (byte & 0xFE)) return false;
				value |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
				if ((byte & 0x80) == 0) return true;
			}
			return false;
		}

		// Sum of the counts the client advertised in field 3, or 0 when the request cannot
		// be understood (the caller then falls back to the fixed retail slot count).
		std::size_t requested_message_slots(const std::string& body)
		{
			std::size_t total{};
			std::size_t categories{};
			std::size_t at{};
			while (at < body.size())
			{
				std::uint64_t tag{};
				if (!read_varint(body, at, tag)) return 0;
				const auto field = tag >> 3;
				switch (tag & 7)
				{
				case 0: // varint
				{
					std::uint64_t value{};
					if (!read_varint(body, at, value)) return 0;
					break;
				}
				case 1: // 64 bit
					if (body.size() - at < 8) return 0;
					at += 8;
					break;
				case 5: // 32 bit
					if (body.size() - at < 4) return 0;
					at += 4;
					break;
				case 2: // length delimited
				{
					std::uint64_t length{};
					if (!read_varint(body, at, length) || length > body.size() - at) return 0;
					const auto end = at + static_cast<std::size_t>(length);
					if (field == 3)
					{
						if (++categories > max_request_categories) return 0;
						std::uint64_t count{};
						auto inner = at;
						while (inner < end)
						{
							std::uint64_t inner_tag{};
							std::uint64_t value{};
							// { 1: category, 2: count }: both varints, nothing else expected.
							if (!read_varint(body, inner, inner_tag) || (inner_tag & 7) != 0) return 0;
							if (!read_varint(body, inner, value)) return 0;
							if ((inner_tag >> 3) == 2) count = value;
						}
						if (inner != end || count > max_slots_per_category) return 0;
						total += static_cast<std::size_t>(count);
					}
					at = end;
					break;
				}
				default:
					return 0;
				}
			}
			return total;
		}

	}

	void bdMarketingComms::getMessages(service_server* server, byte_buffer* buffer) const
	{
		hq_protocol::trace("marketing_6", buffer->get_remaining());
		if (!game::environment::is_zombies())
		{
			std::string request_body{};
			if (!buffer->read_struct(&request_body, 65536) || !hq_protocol::padding(buffer))
			{
				server->create_reply(this->task_id(), BD_PARAM_PARSE_ERROR).send_struct();
				return;
			}

			class bdCommsMessagesResult final : public bdTaskResult
			{
			public:
				std::string payload;

				void serialize(byte_buffer* data) override
				{
					data->write_struct(this->payload.data(), static_cast<int>(this->payload.size()));
				}
			};

			auto slots = requested_message_slots(request_body);
			slots = std::max(slots, default_message_slots);

			auto result = std::make_unique<bdCommsMessagesResult>();
			result->payload = hq_mail::empty_slots(slots);
			byte_buffer encoded;
			result->serialize(&encoded);
			hq_protocol::trace("marketing_6_response", encoded.get_buffer());
			console::info("[HQ mail] getMessages: %zu non-claimable slot(s) for %zu advertised slot(s)\n",
				slots, slots);

			auto reply = server->create_reply(this->task_id());
			reply.add(result);
			reply.send_struct();
			return;
		}
		bdCommsGetMessagesRequest request{};

		class bdCommsGetMessagesResult final : public bdTaskResult
		{
		public:
			bdCommsGetMessagesResponse response;

			void serialize(byte_buffer* data) override
			{
				data->write_struct(&response, sizeof(bdCommsGetMessagesResponse));
			}
		};

		//buffer->read_struct(&request, sizeof(bdCommsGetMessagesRequest));

		auto reply = server->create_reply(this->task_id());

		auto info = std::make_unique<bdCommsGetMessagesResult>();

		unsigned char unk[18]{ 0x0A, 0x10, 0x08, 0x00, 0x12, 0x00, 0x1A, 0x00, 0x22, 0x00, 0x2A, 0x00, 0x32, 0x00, 0x38, 0x00, 0x40, 0x01 };
		for (auto i = 0; i < 1; i++)
		{
			memcpy(info->response.unk[i].__pad0, unk, 17);
		}
		info->response.unk[0].unk = 0x01;

		reply.add(info);

		reply.send_struct();
	}
}
