#include <std_include.hpp>
#include "glutton_server.hpp"
#include "../request_trace.hpp"
#include "../achievement_engine.hpp"

#include "../achievement_response.hpp"
#include "../achievement_store.hpp"

#include "component/console/console.hpp"

#include "steam/steam.hpp"
#include <utils/flags.hpp>
#include <utils/io.hpp>
#include <utils/string.hpp>
#include <atomic>
#include <cctype>
#include <mutex>
#include <set>
#include <string_view>

namespace demonware
{
	namespace
	{
		// One bounded line per unhandled Achievement Engine action per session. The
		// action name comes off the wire, so it is cut and escaped; the body can
		// carry user identifiers, so it only goes to a dump file with
		// -demonware_debug, never to the console log.
		void log_unhandled_action(const std::string_view action, const std::string& body)
		{
			static std::mutex mutex{};
			static std::set<std::string> seen{};
			static std::atomic_uint32_t sequence{};
			constexpr std::size_t maximum_seen = 64;
			constexpr std::size_t maximum_name = 64;
			constexpr std::uint32_t maximum_dumps = 512;

			std::string name{action.substr(0, maximum_name)};
			for (auto& character : name)
			{
				if (static_cast<unsigned char>(character) < 0x20 || character == 0x7F)
				{
					character = '?';
				}
			}

			{
				std::lock_guard lock{mutex};
				if (seen.size() < maximum_seen && seen.insert(name).second)
				{
					console::info("[DW]: [glutton]: unhandled action '%s' (%zu bytes; first this session)\n",
						name.data(), body.size());
				}
			}

			static const auto dump_payloads = utils::flags::has_flag("-demonware_debug");
			if (!dump_payloads || body.empty())
			{
				return;
			}

			std::uint32_t index{};
			if (!request_trace::reserve_dump_slot(sequence, maximum_dumps, index))
			{
				return;
			}

			auto file_name = name;
			for (auto& character : file_name)
			{
				if (!std::isalnum(static_cast<unsigned char>(character)))
				{
					character = '_';
				}
			}

			const auto path = utils::string::va("s2x/dump/dw/glutton_%s_%03u.bin", file_name.data(), index);
			if (utils::io::write_file(path, body))
			{
				console::info("[DW-trace] wrote %s\n", path);
			}
		}
	}
}

namespace demonware
{
	void glutton_server::handle_request(const http_request& request)
	{
		rapidjson::Document response{rapidjson::kObjectType};
		if (request.target.find("/secureingest/") == std::string::npos)
		{
			const auto json = achievement_engine::dispatch(request.body);
			response.Parse(json.data(), json.size());
		}
		send_json(response);
	}
}
