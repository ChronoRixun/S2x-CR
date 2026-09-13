#pragma once

#include "reward_game_event.hpp"
#include <charconv>
#include <limits>
#include <string_view>

namespace demonware::hq_event_relay
{
	inline constexpr std::string_view command = "s2x_hq";
	inline constexpr std::size_t maximum_parameters = 16, maximum_wire = 768;

	template <typename T>
	bool number(const std::string_view text, T& value)
	{
		if (text.empty()) return false;
		const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
		return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
	}

	inline bool decode(std::string_view wire, const std::uint64_t local_user, reward_game_events::event& result)
	{
		if (wire.empty() || wire.size() > maximum_wire || !local_user) return false;
		const auto token = [&wire]()
		{
			const auto end = wire.find(' ');
			const auto value = wire.substr(0, end);
			wire = end == std::string_view::npos ? std::string_view{} : wire.substr(end + 1);
			return value;
		};
		std::uint64_t user{};
		unsigned version{}, count{};
		reward_game_events::event parsed{};
		if (token() != command || !number(token(), version) || version != 1 ||
			!number(token(), user) || user != local_user || !number(token(), parsed.timestamp) || parsed.timestamp < 0) return false;
		const auto name = token();
		if (name.empty() || name.size() > 64) return false;
		for (const auto byte : name)
			if ((byte < '0' || byte > '9') && (byte < 'a' || byte > 'z') && byte != '_') return false;
		if (!number(token(), count) || count > maximum_parameters) return false;
		parsed.name = name;
		for (unsigned i = 0; i < count; ++i)
		{
			unsigned selector{};
			std::uint64_t value{};
			if (!number(token(), selector) || selector == 0 || selector > 64 || !number(token(), value)) return false;
			for (const auto& prior : parsed.parameters) if (prior.selector == std::to_string(selector)) return false;
			parsed.parameters.push_back({std::to_string(selector), value});
		}
		if (!wire.empty()) return false;
		result = std::move(parsed);
		return true;
	}

	inline std::string encode(const std::uint64_t user, const reward_game_events::event& event)
	{
		if (event.name.empty() || event.name.size() > 64 || event.parameters.size() > maximum_parameters) return {};
		std::string wire = std::string{command} + " 1 " + std::to_string(user) + " " +
			std::to_string(event.timestamp) + " " + event.name + " " + std::to_string(event.parameters.size());
		for (const auto& parameter : event.parameters)
		{
			if (parameter.selector.size() > 2) return {};
			wire += " " + parameter.selector + " " + std::to_string(parameter.value);
		}
		reward_game_events::event checked{};
		return decode(wire, user, checked) ? wire : std::string{};
	}

	// The receiver and harness use exactly this validation-before-apply path.
	template <typename Submit>
	bool apply(const std::string_view wire, const std::uint64_t local_user, Submit&& submit)
	{
		reward_game_events::event event{};
		return decode(wire, local_user, event) && submit(event);
	}
}
