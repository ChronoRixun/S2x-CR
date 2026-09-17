#pragma once

#include "reward_game_event.hpp"
#include <charconv>
#include <array>
#include <algorithm>
#include <limits>
#include <string_view>

namespace demonware::hq_event_relay
{
	// CG_DeployServerCommandString (0x431F1D) ignores first bytes above 0x7C.
	// Tilde keeps stock clients out of the engine's single-byte opcode handlers.
	inline constexpr std::string_view command = "~s2x_hq";
	inline constexpr std::size_t maximum_parameters = 256, maximum_wire = 8192;
	// SV_AddServerCommand (0x6DDFE0) copies into 0x400-byte slots, including NUL.
	inline constexpr std::size_t maximum_command = 1023, chunk_bytes = 400;
	inline constexpr std::size_t maximum_chunks = (maximum_wire + chunk_bytes - 1) / chunk_bytes;

	template <typename T>
	bool number(const std::string_view text, T& value)
	{
		if (text.empty()) return false;
		const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
		return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
	}

	inline bool decode(std::string_view wire, const std::uint64_t local_user, reward_game_events::event& result)
	{
		if (wire.empty() || wire.back() == ' ' || wire.size() > maximum_wire || !local_user) return false;
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
		if (name.empty() || name.size() > 99) return false;
		for (const auto byte : name)
			if ((byte < '0' || byte > '9') && (byte < 'a' || byte > 'z') && (byte < 'A' || byte > 'Z') && byte != '_') return false;
		if (!number(token(), count) || count > maximum_parameters) return false;
		parsed.name = name;
		for (unsigned i = 0; i < count; ++i)
		{
			unsigned selector{};
			std::uint64_t value{};
			if (!number(token(), selector) || selector > 255 || !number(token(), value)) return false;
			for (const auto& prior : parsed.parameters) if (prior.selector == std::to_string(selector)) return false;
			parsed.parameters.push_back({std::to_string(selector), value});
		}
		if (!wire.empty()) return false;
		result = std::move(parsed);
		return true;
	}

	inline std::string encode(const std::uint64_t user, const reward_game_events::event& event)
	{
		if (event.name.empty() || event.name.size() > 99 || event.parameters.size() > maximum_parameters) return {};
		std::string wire = std::string{command} + " 1 " + std::to_string(user) + " " +
			std::to_string(event.timestamp) + " " + event.name + " " + std::to_string(event.parameters.size());
		for (const auto& parameter : event.parameters)
		{
			if (parameter.selector.size() > 3) return {};
			wire += " " + parameter.selector + " " + std::to_string(parameter.value);
		}
		reward_game_events::event checked{};
		return decode(wire, user, checked) ? wire : std::string{};
	}

	inline std::uint64_t fingerprint(const std::string_view wire)
	{
		std::uint64_t hash = 14695981039346656037ULL;
		for (const auto byte : wire) { hash ^= static_cast<unsigned char>(byte); hash *= 1099511628211ULL; }
		return hash;
	}

	inline std::vector<std::string> chunks(const std::uint64_t user, const std::string_view wire)
	{
		reward_game_events::event checked{};
		if (!decode(wire, user, checked)) return {};
		const auto total = (wire.size() + chunk_bytes - 1) / chunk_bytes;
		std::vector<std::string> result;
		for (std::size_t index = 0; index < total; ++index)
		{
			auto part = std::string{command} + " 2 " + std::to_string(user) + " " +
				std::to_string(fingerprint(wire)) + " " + std::to_string(index) + " " + std::to_string(total) + " ";
			for (const unsigned char byte : wire.substr(index * chunk_bytes, chunk_bytes))
			{
				part += "0123456789abcdef"[byte >> 4];
				part += "0123456789abcdef"[byte & 15];
			}
			if (part.size() > maximum_command) return {};
			result.push_back(std::move(part));
		}
		return result;
	}

	// Within one uninterrupted connection, reliable fragments arrive in order and
	// server_queue drains events one at a time per client. The receiver may lag behind
	// selection: its next fragment continues its partial or is a fragment zero that
	// replaces it. Paused sends resume at the next fragment, never orphaning a partial.
	// Keep one bounded partial without a deadline; reconnect recovery is not guaranteed.
	class receiver
	{
	public:
		template <typename Submit>
		bool accept(std::string_view part, const std::uint64_t user, Submit&& submit)
		{
			const auto reject = [this]() { reset(); return false; };
			if (!user || part.empty() || part.size() > maximum_command) return reject();
			const auto token = [&part]()
			{
				const auto end = part.find(' ');
				const auto value = part.substr(0, end);
				part = end == std::string_view::npos ? std::string_view{} : part.substr(end + 1);
				return value;
			};
			std::uint64_t recipient{}, hash{};
			unsigned version{}, index{}, total{};
			if (token() != command || !number(token(), version) || version != 2 ||
				!number(token(), recipient) || recipient != user || !number(token(), hash) ||
				!number(token(), index) || !number(token(), total) || !total || total > maximum_chunks || index >= total)
				return reject();
			if (part.empty() || part.size() > chunk_bytes * 2 || part.size() % 2 ||
				(index + 1 < total && part.size() != chunk_bytes * 2)) return reject();
			if (index == 0)
			{
				reset(); user_ = user; hash_ = hash; total_ = total;
			}
			// After header/length validation, wrong-identity/index continuations preserve the partial.
			// Earlier validation failures (and invalid hex below) reset it.
			if (user != user_ || hash != hash_ || total != total_ || index != next_) return false;
			const auto hex = [](const char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
			for (std::size_t i = 0; i < part.size(); i += 2)
			{
				const auto high = hex(part[i]), low = hex(part[i + 1]);
				if (high < 0 || low < 0 || wire_.size() == maximum_wire) return reject();
				wire_ += static_cast<char>((high << 4) | low);
			}
			if (++next_ != total_) return true;
			auto wire = std::move(wire_);
			reset();
			reward_game_events::event event{};
			return fingerprint(wire) == hash && decode(wire, user, event) && submit(event);
		}

	private:
		void reset() { wire_.clear(); user_ = hash_ = 0; next_ = total_ = 0; }
		std::string wire_;
		std::uint64_t user_{}, hash_{};
		unsigned next_{}, total_{};
	};

	// The receiver and harness use exactly this validation-before-apply path.
	template <typename Submit>
	bool apply(const std::string_view wire, const std::uint64_t local_user, Submit&& submit)
	{
		reward_game_events::event event{};
		return decode(wire, local_user, event) && submit(event);
	}
}
